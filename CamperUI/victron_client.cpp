#include "victron_client.h"
#include "system_state.h"
#include "PubSubClient.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <math.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {
VictronSnapshot model;
GxProbe probes[3];
QueueHandle_t commands=nullptr; TaskHandle_t worker=nullptr;
uint32_t config_revision=1,last_click[3]={}; bool clicked[3]={};
struct Command {uint8_t channel;int target;uint32_t session;};
struct Config {
    bool enabled=false,demo=false,keepalive=false,starter=false;int battery_instance=-1;
    String host,portal,user,password;uint16_t port=1883;int instance=-1;
    bool same_target(const Config &b)const {
        return enabled==b.enabled&&demo==b.demo&&host==b.host&&portal==b.portal&&user==b.user&&
            password==b.password&&port==b.port&&instance==b.instance&&starter==b.starter&&battery_instance==b.battery_instance;
    }
};
Config config;
WiFiClient *tcp=nullptr; PubSubClient *mqtt=nullptr;
GxSelection selection,starter_selection;int subscribed_battery=-1;bool starter_subscribed=false;
bool keepalive_refreshed=false;
char subscribed_portal[33]={};int subscribed_instance=-1,read_step=0;
uint32_t retry_at=0,next_read=0,last_keepalive=0;
const char *vebus_paths[]={"Mode","ModeIsAdjustable","Connected","State",
    "Ac/ActiveIn/L1/V","Ac/ActiveIn/L1/I","Ac/ActiveIn/L1/P","Ac/ActiveIn/P",
    "Ac/Out/L1/V","Ac/Out/L1/I","Ac/Out/L1/P","Ac/Out/P",
    "Ac/ActiveIn/L1/F","Ac/Out/L1/F","Dc/0/Voltage","Dc/0/Current"};
void message(const char *s) { snprintf(model.message,sizeof(model.message),"%s",s); }
void reset_session(const Config &next,const char *reason) {
    if(mqtt)mqtt->disconnect();
    StateLockGuard lock;
    uint32_t epoch=model.session+1; model=VictronSnapshot();model.session=epoch;
    model.enabled=next.enabled;model.demo=next.demo;
    for(auto &probe:probes)probe=GxProbe();
    snprintf(model.portal,sizeof(model.portal),"%s",next.portal.c_str());
    model.instance=next.instance;
    selection=GxSelection();starter_selection=GxSelection();subscribed_battery=-1;starter_subscribed=false;keepalive_refreshed=false;
    model.battery_instance=next.battery_instance;
    subscribed_portal[0]=0;subscribed_instance=-1;read_step=0;
    if(commands)xQueueReset(commands);
    message(reason);
}
Config read_config() {
    StateLockGuard lock;Config c;
    c.enabled=state.victron_enabled;c.demo=state.debug_mode;c.keepalive=state.victron_keepalive;
    c.starter=state.victron_starter;c.battery_instance=state.victron_battery_instance;
    c.host=state.victron_host;c.portal=state.victron_portal;c.user=state.victron_user;
    c.password=state.victron_password;c.port=state.victron_port;c.instance=state.victron_instance;return c;
}
bool choose_portal(const char *portal) {
    if(config.portal.length())return config.portal==portal;
    if(!model.portal[0]) { snprintf(model.portal,sizeof(model.portal),"%s",portal);return true; }
    if(strcmp(model.portal,portal)) {selection.portal_conflict=true;model.ambiguous=true;message("Mehrere Portal-IDs: bitte fest zuordnen");return false;}
    return true;
}
void choose_instance(int instance,bool preferred=false) {
    if(config.instance>=0)return;
    model.instance=selection.observe(instance,preferred);
    model.ambiguous=selection.ambiguous();
    if(model.ambiguous && !selection.portal_conflict)message("Mehrere VE.Bus-Geraete: ID fest zuordnen");
}
void received(char *topic,byte *payload,unsigned length) {
    GxTopic t=gx_parse_topic(topic);if(!t.valid)return;
    bool retained=mqtt->messageRetained();
    JsonDocument doc;bool parsed=length && !deserializeJson(doc,payload,length);
    JsonVariant value=doc["value"];
    bool numeric=parsed && !value.isNull() && !value.is<bool>() && (value.is<int>()||value.is<float>());
    float number=numeric?value.as<float>():0;numeric=numeric&&isfinite(number);
    if(parsed && value.is<bool>() &&
       ((gx_equal(t.service,"vebus")&&(gx_equal(t.path,"Connected")||gx_equal(t.path,"ModeIsAdjustable"))) ||
        (gx_equal(t.service,"system")&&(gx_equal(t.path,"Relay/0/State")||gx_equal(t.path,"Relay/1/State"))) ||
        (gx_equal(t.service,"battery")&&gx_equal(t.path,"Connected")))) {
        number=value.as<bool>()?1:0;numeric=true;
    }
    uint32_t now=millis();StateLockGuard lock;
    if(gx_equal(t.service,"battery")&&!config.starter)return;
    if(!choose_portal(t.portal))return;
    if(t.heartbeat) {if(!retained){model.last_message=now;model.stale=false;}return;}
    int field=-1;
    if(gx_equal(t.service,"system")) {
        if(model.system_instance<0)model.system_instance=t.instance;
        if(t.instance!=model.system_instance)return;
        if(gx_equal(t.path,"VebusInstance")&&numeric&&number>=0&&number<=65535&&number==floorf(number))choose_instance((int)number,true);
        if(gx_equal(t.path,"Relay/0/State"))field=GX_RELAY0;
        if(gx_equal(t.path,"Relay/1/State"))field=GX_RELAY1;
        if(gx_equal(t.path,"Dc/Battery/Soc"))field=GX_SOC;
        if(gx_equal(t.path,"Dc/Pv/Power"))field=GX_PV;
    } else if(gx_equal(t.service,"battery")) {
        if(gx_equal(t.path,"Dc/1/Voltage") && numeric && number>0 && number<=1000 && config.battery_instance<0) {
            model.battery_instance=starter_selection.observe(t.instance);
            model.starter_ambiguous=starter_selection.ambiguous();
            if(model.starter_ambiguous) {model.values[GX_STARTER]=GxValue();model.values[GX_STARTER_CONNECTED]=GxValue();return;}
        }
        if(t.instance!=model.battery_instance || model.starter_ambiguous)return;
        if(gx_equal(t.path,"Dc/1/Voltage"))field=GX_STARTER;
        if(gx_equal(t.path,"Connected"))field=GX_STARTER_CONNECTED;
    } else if(gx_equal(t.service,"settings")) {
        if(gx_equal(t.path,"Settings/Relay/Function"))field=GX_FUNCTION0;
        if(gx_equal(t.path,"Settings/Relay/1/Function"))field=GX_FUNCTION1;
        if(field>=0)model.settings_instance=t.instance;
    } else if(gx_equal(t.service,"vebus")) {
        if((gx_equal(t.path,"ProductName")&&value.is<const char*>()) || (gx_equal(t.path,"Mode")&&numeric))choose_instance(t.instance);
        if(t.instance!=model.instance)return;
        if(gx_equal(t.path,"ProductName")&&value.is<const char*>())
            snprintf(model.product,sizeof(model.product),"%s",value.as<const char*>());
        for(int i=0;i<16;++i)if(gx_equal(t.path,vebus_paths[i]))field=i;
    }
    if(field<0)return;
    if(field==GX_MODE)numeric=numeric&&(number==1||number==2||number==3||number==4);
    if(field==GX_ADJUSTABLE||field==GX_DEVICE_CONNECTED||field==GX_RELAY0||field==GX_RELAY1||field==GX_STARTER_CONNECTED)
        numeric=numeric&&(number==0||number==1);
    if(field==GX_FUNCTION0||field==GX_FUNCTION1)numeric=numeric&&number>=0&&number<=16&&number==floorf(number);
    if(field==GX_STATE)numeric=numeric&&number>=0&&number<=1000&&number==floorf(number);
    if(field==GX_STARTER)numeric=numeric&&number>0&&number<=1000;
    if(field==GX_SOC)numeric=numeric&&number>=0&&number<=100;
    if(numeric && (field==GX_IN_V||field==GX_OUT_V||field==GX_IN_F||field==GX_OUT_F||field==GX_DC_V))
        numeric=number>=0&&number<=1000;
    if(numeric&&fabsf(number)>1000000)numeric=false;
    model.values[field]={number,now,numeric,!retained};
    if(!retained) {model.last_message=now;model.stale=false;}
    if(numeric) {
        if(field==GX_MODE)probes[0].observe(1,retained,model.session,now);
        if(field==GX_ADJUSTABLE)probes[0].observe(2,retained,model.session,now);
        if(field==GX_DEVICE_CONNECTED)probes[0].observe(4,retained,model.session,now);
        for(int r=0;r<2;++r) {
            if(field==GX_RELAY0+r)probes[r+1].observe(1,retained,model.session,now);
            if(field==GX_FUNCTION0+r)probes[r+1].observe(2,retained,model.session,now);
        }
    }
    int channel=field==GX_MODE?0:field==GX_RELAY0?1:field==GX_RELAY1?2:-1;
    if(channel>=0 && numeric && model.pending[channel].observe((int)number,retained,model.session,now))
        message("Zustand bestaetigt");
}
bool subscribe(const char *service,const char *instance,const char *path) {
    char topic[192];snprintf(topic,sizeof(topic),"N/%s/%s/%s/%s",model.portal,service,instance,path);
    return mqtt->subscribe(topic,0);
}
bool request_read(const char *service,int instance,const char *path) {
    char topic[192];snprintf(topic,sizeof(topic),"R/%s/%s/%d/%s",model.portal,service,instance,path);
    return mqtt->publish(topic,"",false);
}
void subscriptions() {
    if(!model.portal[0]||model.ambiguous)return;
    if(strcmp(subscribed_portal,model.portal)) {
        snprintf(subscribed_portal,sizeof(subscribed_portal),"%s",model.portal);
        const char *system_paths[]={"VebusInstance","Relay/0/State","Relay/1/State","Dc/Battery/Soc","Dc/Pv/Power"};
        for(auto p:system_paths)subscribe("system","+",p);
        subscribe("settings","+","Settings/Relay/Function");subscribe("settings","+","Settings/Relay/1/Function");
        char full[80];snprintf(full,sizeof(full),"N/%s/full_publish_completed",model.portal);mqtt->subscribe(full,0);
        read_step=1;next_read=0;last_keepalive=millis()-30000;
    }
    if(model.instance>=0 && subscribed_instance!=model.instance) {
        if(subscribed_instance>=0) {
            char id[12];snprintf(id,sizeof(id),"%d",subscribed_instance);
            for(auto p:vebus_paths) {char topic[192];snprintf(topic,sizeof(topic),"N/%s/vebus/%s/%s",model.portal,id,p);mqtt->unsubscribe(topic);}
            char product_topic[192];snprintf(product_topic,sizeof(product_topic),"N/%s/vebus/%s/ProductName",model.portal,id);mqtt->unsubscribe(product_topic);
            StateLockGuard lock;
            for(int i=0;i<16;++i)model.values[i]=GxValue();
            for(auto &p:model.pending)if(p.active())p.phase=GxCommandPhase::Failed;
            ++model.session;if(commands)xQueueReset(commands);
        }
        subscribed_instance=model.instance;
        char id[12];snprintf(id,sizeof(id),"%d",model.instance);
        for(auto p:vebus_paths)subscribe("vebus",id,p);
        subscribe("vebus",id,"ProductName");
        if(read_step==0 || read_step>=8)read_step=8;next_read=0;
    }
    if(config.starter && !starter_subscribed) {
        char id[12];snprintf(id,sizeof(id),"%d",config.battery_instance);
        subscribe("battery",config.battery_instance<0?"+":id,"Dc/1/Voltage");
        subscribe("battery",config.battery_instance<0?"+":id,"Connected");
        starter_subscribed=true;
    }
    if(config.starter && model.battery_instance>=0 && subscribed_battery!=model.battery_instance) {
        subscribed_battery=model.battery_instance;
        {StateLockGuard lock;model.values[GX_STARTER_CONNECTED]=GxValue();}
        request_read("battery",subscribed_battery,"Connected");request_read("battery",subscribed_battery,"Dc/1/Voltage");
    }
    uint32_t now=millis();
    if(read_step>0 && (int32_t)(now-next_read)>=0) {
        int si=model.system_instance<0?0:model.system_instance;
        const char *system_paths[]={"VebusInstance","Relay/0/State","Relay/1/State","Dc/Battery/Soc","Dc/Pv/Power"};
        if(read_step<=5)request_read("system",si,system_paths[read_step-1]);
        else if(read_step<=7)request_read("settings",model.settings_instance,read_step==6?"Settings/Relay/Function":"Settings/Relay/1/Function");
        else if(model.instance>=0 && read_step<=23)request_read("vebus",model.instance,vebus_paths[read_step-8]);
        else if(model.instance>=0&&read_step==24)request_read("vebus",model.instance,"ProductName");
        else {read_step=0;return;}
        ++read_step;next_read=now+50;
    }
    if(config.keepalive && now-last_keepalive>=30000) {
        char topic[96];snprintf(topic,sizeof(topic),"R/%s/keepalive",model.portal);
        bool first=!keepalive_refreshed || model.stale;
        if(first) {StateLockGuard lock;for(auto &value:model.values)value.live=false;}
        keepalive_refreshed=true;
        mqtt->publish(topic,first?"":"{\"keepalive-options\":[\"suppress-republish\"]}",false);
        // A single control read also verifies liveness for otherwise static data.
        if(model.instance>=0)request_read("vebus",model.instance,"Mode");
        last_keepalive=now;
    }
}

bool mode_control_allowed(const VictronSnapshot &s) {
    const auto &mode=s.values[GX_MODE],&connected=s.values[GX_DEVICE_CONNECTED],&permission=s.values[GX_ADJUSTABLE];
    return gx_mode_control_allowed(s.enabled&&!s.stale&&!s.ambiguous&&(s.demo||s.connected),s.instance,
        mode.valid,mode.live,mode.valid?(int)mode.value:0,
        connected.valid,connected.live,connected.valid?(int)connected.value:0,
        permission.valid,permission.live,permission.valid?(int)permission.value:0);
}
bool target_current() {
    return state.victron_enabled && state.debug_mode==model.demo &&
        state.victron_host==config.host && state.victron_portal==config.portal &&
        state.victron_instance==config.instance && state.victron_port==config.port &&
        state.victron_user==config.user && state.victron_password==config.password &&
        model.enabled&&!model.stale&&!model.ambiguous&&(model.demo||model.connected);
}
void process_command() {
    Command command;
    if(commands && xQueueReceive(commands,&command,0)==pdTRUE) {
        StateLockGuard lock;
        if(command.channel<=2 && command.session==model.session && model.pending[command.channel].phase==GxCommandPhase::Queued) {
            auto &pending=model.pending[command.channel];
            if(!target_current()) {pending.phase=GxCommandPhase::Failed;message("Befehl verworfen: Verbindung geaendert");}
            else if(model.demo) {
                pending.mark_sent(model.session,millis());
                int field=command.channel==0?GX_MODE:GX_RELAY0+command.channel-1;
                model.values[field]={(float)command.target,millis(),true,true};
                pending.observe(command.target,false,model.session,millis());
                if(command.channel==0)model.values[GX_STATE]={(float)(command.target==4?0:command.target==1?3:8),millis(),true,true};
                message("Demo: Zustand bestaetigt");
            } else {
                probes[command.channel].begin(command.channel==0?7:3,model.session,millis());
                // Verify current permissions/role AND state before each write.
                state_unlock();
                if(command.channel==0) {
                    request_read("vebus",model.instance,"Mode");
                    request_read("vebus",model.instance,"ModeIsAdjustable");
                    request_read("vebus",model.instance,"Connected");
                } else {
                    char path[32];snprintf(path,sizeof(path),"Relay/%d/State",command.channel-1);
                    request_read("system",model.system_instance,path);
                    request_read("settings",model.settings_instance,command.channel==1?"Settings/Relay/Function":"Settings/Relay/1/Function");
                }
                state_lock();
            }
        }
    }
    for(int channel=0;channel<3;++channel) {
        StateLockGuard lock;auto &probe=probes[channel];auto &pending=model.pending[channel];
        if(!probe.active)continue;
        if(probe.session!=model.session || pending.phase!=GxCommandPhase::Queued || pending.expire(millis())) {
            probe.active=false;if(pending.phase==GxCommandPhase::Timeout)message("Keine Bestaetigung: Zustand pruefen");continue;
        }
        if(!probe.complete())continue;
        probe.active=false;
        bool ready=target_current();
        if(channel==0)ready=ready && mode_control_allowed(model);
        else ready=ready && victron_fresh(model,(GxField)(GX_FUNCTION0+channel-1))&&
            model.values[GX_FUNCTION0+channel-1].value==2&&victron_fresh(model,(GxField)(GX_RELAY0+channel-1));
        if(!ready) {pending.phase=GxCommandPhase::Failed;message(channel==0?"Fernsteuerung nicht freigegeben / MultiPlus getrennt":"GX-Relais ist nicht manuell oder nicht verfuegbar");continue;}
        int field=channel==0?GX_MODE:GX_RELAY0+channel-1;
        if(model.values[field].value==pending.target) {
            pending.phase=GxCommandPhase::Confirmed;message("Aktueller Zustand bereits bestaetigt");continue;
        }
        char topic[192],payload[32],path[32];
        if(channel==0)snprintf(topic,sizeof(topic),"W/%s/vebus/%d/Mode",model.portal,model.instance);
        else {snprintf(path,sizeof(path),"Relay/%d/State",channel-1);
            snprintf(topic,sizeof(topic),"W/%s/system/%d/%s",model.portal,model.system_instance,path);}
        snprintf(payload,sizeof(payload),"{\"value\":%d}",pending.target);
        state_unlock();bool sent=mqtt->publish(topic,payload,false);state_lock();
        if(!sent){pending.phase=GxCommandPhase::Failed;message("MQTT-Senden fehlgeschlagen");continue;}
        pending.mark_sent(model.session,millis());message("Warte auf Rueckmeldung");
        state_unlock();request_read(channel==0?"vebus":"system",channel==0?model.instance:model.system_instance,channel==0?"Mode":path);state_lock();
    }
}
void demo_tick() {
    uint32_t now=millis();float phase=(now%180000)*2*PI/180000;
    StateLockGuard lock;model.connected=false;model.stale=false;model.last_message=now;
    if(!model.values[GX_MODE].valid) {
        model.values[GX_MODE]={3.f,now,true,true};model.values[GX_STATE]={8.f,now,true,true};
        model.values[GX_RELAY0]={0.f,now,true,true};model.values[GX_RELAY1]={0.f,now,true,true};
    }
    model.values[GX_ADJUSTABLE]={1.f,now,true,true};model.values[GX_DEVICE_CONNECTED]={1.f,now,true,true};
    model.values[GX_FUNCTION0]={2.f,now,true,true};model.values[GX_FUNCTION1]={2.f,now,true,true};
    bool source_ready=state.data_is_demo==state.debug_mode;
    float load=140+30*sinf(phase);bool off=model.values[GX_MODE].value==4;
    model.values[GX_IN_V]={230.f,now,true,true};model.values[GX_IN_I]={off?0.f:2.2f,now,true,true};
    model.values[GX_IN_P]={off?0.f:506.f,now,true,true};model.values[GX_IN_TOTAL]=model.values[GX_IN_P];
    model.values[GX_OUT_V]={off?0.f:230.f,now,true,true};model.values[GX_OUT_I]={off?0.f:load/230,now,true,true};
    model.values[GX_OUT_P]={off?0.f:load,now,true,true};model.values[GX_OUT_TOTAL]=model.values[GX_OUT_P];
    model.values[GX_IN_F]={50.f,now,true,true};model.values[GX_OUT_F]={off?0.f:50.f,now,true,true};
    // Reuse the existing shared demo battery/solar values.
    model.values[GX_DC_V]={state.bat_voltage,now,source_ready&&bool(state.battery_fields&1),true};
    model.values[GX_DC_I]={state.bat_current,now,source_ready&&bool(state.battery_fields&2),true};
    model.values[GX_SOC]={(float)state.bat_soc,now,source_ready&&bool(state.battery_fields&4),true};
    model.values[GX_PV]={state.solar_power,now,source_ready&&bool(state.solar_fields&1),true};
    model.values[GX_STARTER]={state.starter_voltage,now,config.starter&&source_ready&&bool(state.battery_fields&8),true};
    model.values[GX_STARTER_CONNECTED]={1.f,now,config.starter,true};
    snprintf(model.product,sizeof(model.product),"MultiPlus-II (Demo)");
}
void background(void*) {
    WiFiClient connection;PubSubClient client(connection);tcp=&connection;mqtt=&client;
    while(!client.setBufferSize(768)) {
        {StateLockGuard lock;model.enabled=state.victron_enabled;message("MQTT-Puffer nicht verfuegbar");}
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    client.setSocketTimeout(1);client.setKeepAlive(30);client.setCallback(received);
    uint32_t seen_revision=0,last_demo=0;
    for(;;) {
        uint32_t revision;bool demo;{StateLockGuard lock;revision=config_revision;demo=state.debug_mode;}
        if(revision!=seen_revision || seen_revision==0 || demo!=config.demo) {
            Config next=read_config();
            if(!config.same_target(next) || !seen_revision) {reset_session(next,next.enabled?"Verbinde mit Cerbo GX":"Victron deaktiviert");retry_at=0;}
            if(next.keepalive!=config.keepalive){last_keepalive=millis()-30000;keepalive_refreshed=false;}
            config=next;seen_revision=revision;
        }
        if(!config.enabled || state.web_ota_active) {
            if(client.connected())client.disconnect();
            {StateLockGuard lock;model.connected=false;model.stale=true;
                if(config.enabled)message("MQTT pausiert: Firmware-Update");
                for(auto &p:model.pending)if(p.active())p.phase=GxCommandPhase::Failed;
                for(auto &p:probes)p.active=false;
                if(commands)xQueueReset(commands);}
            vTaskDelay(pdMS_TO_TICKS(200));continue;
        }
        uint32_t now=millis();
        if(config.demo) {
            if(now-last_demo>=500){demo_tick();last_demo=now;}process_command();
            vTaskDelay(pdMS_TO_TICKS(30));continue;
        }
        if(WiFi.status()!=WL_CONNECTED || !config.host.length()) {
            if(client.connected())client.disconnect();
            {StateLockGuard lock;model.connected=false;model.stale=true;
                message(config.host.length()?"WLAN getrennt":"Cerbo-IP oder Hostnamen eintragen");
                for(auto &p:model.pending)if(p.active())p.phase=GxCommandPhase::Failed;
                for(auto &p:probes)p.active=false;
                if(commands)xQueueReset(commands);}
            vTaskDelay(pdMS_TO_TICKS(150));continue;
        }
        if(!client.connected()) {
            {StateLockGuard lock;model.connected=false;model.stale=true;}
            if((int32_t)(now-retry_at)<0){vTaskDelay(pdMS_TO_TICKS(100));continue;}
            reset_session(config,"MQTT-Verbindung wird aufgebaut");
            IPAddress ip;bool resolved=ip.fromString(config.host.c_str()) || WiFi.hostByName(config.host.c_str(),ip)==1;
            bool ok=false;
            if(resolved && connection.connect(ip,config.port,250)) {
                client.setServer(ip,config.port);
                char id[48];snprintf(id,sizeof(id),"camperui-%llx",(unsigned long long)ESP.getEfuseMac());
                ok=config.user.length()?client.connect(id,config.user.c_str(),config.password.c_str()):client.connect(id);
            }
            if(ok) {
                client.subscribe("N/+/system/+/Serial",0);client.subscribe("N/+/system/+/VebusInstance",0);
                client.subscribe("N/+/vebus/+/ProductName",0);client.subscribe("N/+/vebus/+/Mode",0);
                client.subscribe("N/+/keepalive",0);
                StateLockGuard lock;model.connected=true;message("Verbunden: warte auf GX-Daten");
            } else {
                connection.stop();retry_at=millis()+10000;
                StateLockGuard lock;
                message(client.state()==4||client.state()==5?"MQTT-Anmeldung abgelehnt: Zugang pruefen":"GX nicht erreichbar: Adresse, Port und MQTT pruefen");
            }
        }
        if(client.connected()) {
            client.loop();subscriptions();process_command();
            StateLockGuard lock;model.connected=client.connected();
            model.stale=!model.connected || !model.last_message || millis()-model.last_message>120000;
            if(!model.connected) {
                message("MQTT-Verbindung verloren");
                for(auto &p:model.pending)if(p.active())p.phase=GxCommandPhase::Failed;
                for(auto &p:probes)p.active=false;
                if(commands)xQueueReset(commands);
            }
            for(auto &p:model.pending)if(p.expire(millis()))message("Keine Bestaetigung: Zustand pruefen");
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
bool request(uint8_t channel,int target) {
    StateLockGuard lock;
    if(!state.victron_enabled || state.debug_mode!=model.demo || !commands)return false;
    bool allowed=channel==0?victron_mode_available(model):victron_relay_available(model,channel-1);
    if(!allowed)return false;
    uint32_t now=millis();if(clicked[channel]&&now-last_click[channel]<600)return false;
    Command command={channel,target,model.session};
    model.pending[channel].begin(target,model.session,now);
    if(xQueueSend(commands,&command,0)!=pdTRUE) {model.pending[channel].phase=GxCommandPhase::Failed;message("Befehlsqueue voll");return false;}
    clicked[channel]=true;last_click[channel]=now;message("Warte auf Rueckmeldung");return true;
}
}
void victron_settings_changed(){StateLockGuard lock;++config_revision;}
void victron_init(){victron_tick();}
void victron_tick() {
    StateLockGuard lock;
    if(worker || !state.victron_enabled)return;
    if(!commands)commands=xQueueCreate(4,sizeof(Command));
    if(!commands || xTaskCreatePinnedToCore(background,"victron",6144,nullptr,1,&worker,0)!=pdPASS) {
        worker=nullptr;model.enabled=state.victron_enabled;message("Victron-Worker: zu wenig Speicher");
    }
}
VictronSnapshot victron_snapshot(){StateLockGuard lock;return model;}
bool victron_fresh(const VictronSnapshot &s,GxField f) {
    if(f==GX_STARTER && (s.starter_ambiguous || !s.values[GX_STARTER_CONNECTED].valid ||
       !s.values[GX_STARTER_CONNECTED].live || s.values[GX_STARTER_CONNECTED].value!=1))return false;
    return s.enabled&&!s.stale&&!s.ambiguous&&(s.demo||s.connected)&&s.values[f].valid&&s.values[f].live &&
        (f==GX_MODE || f==GX_ADJUSTABLE || f==GX_DEVICE_CONNECTED || f>=GX_SOC || s.demo || (s.values[GX_DEVICE_CONNECTED].valid && s.values[GX_DEVICE_CONNECTED].value==1));
}
bool victron_mode_available(const VictronSnapshot &s) {
    return (mode_control_allowed(s)&&!s.pending[0].active()) ||
        (s.demo&&s.enabled&&!s.stale&&!s.pending[0].active());
}
bool victron_relay_available(const VictronSnapshot &s,int relay) {
    if(relay<0||relay>1)return false;
    return victron_fresh(s,(GxField)(GX_RELAY0+relay))&&victron_fresh(s,(GxField)(GX_FUNCTION0+relay))&&
        s.values[GX_FUNCTION0+relay].value==2&&(s.demo||s.system_instance>=0)&&!s.pending[relay+1].active();
}
bool victron_request_mode(int mode){return (mode==1||mode==3||mode==4)&&request(0,mode);}
bool victron_request_relay(int relay,bool on){return relay>=0&&relay<2&&request(relay+1,on?1:0);}
const char *victron_mode_name(int mode) {
    switch(mode){case 1:return "Nur Laden";case 2:return "Nur Wechselrichter";case 3:return "Ein";case 4:return "Aus";default:return "--";}
}
const char *victron_state_name(int state) {
    switch(state){case 0:return "Aus";case 1:return "Standby";case 2:return "Fehler";case 3:return "Bulk-Laden";
    case 4:return "Absorption";case 5:return "Float";case 6:return "Erhaltung";case 7:return "Ausgleich";
    case 8:return "Durchleitung";case 9:return "Wechselrichter";case 10:return "PowerAssist";case 11:return "Netzteil";
    case 244:return "Sustain";case 252:return "Extern geregelt";default:return "Anderer Zustand";}
}
const char *victron_status(const VictronSnapshot &s) {
    if(!s.enabled)return "Victron deaktiviert";
    if(s.ambiguous)return s.message;
    if(!s.demo && !s.connected)return s.message;
    if(s.stale)return "Daten fehlen/veraltet: Keepalive pruefen";
    for(const auto &p:s.pending)if(p.active())return "Warte auf Rueckmeldung";
    for(const auto &p:s.pending)if(p.phase==GxCommandPhase::Timeout)return "Keine Bestaetigung: Zustand pruefen";
    for(const auto &p:s.pending)if(p.phase==GxCommandPhase::Failed)return "Ein Steuerbefehl fehlgeschlagen: Zustand pruefen";
    if(s.demo)return "Demo / keine MQTT-Schaltbefehle";
    const auto &mode=s.values[GX_MODE],&connected=s.values[GX_DEVICE_CONNECTED],&permission=s.values[GX_ADJUSTABLE];
    switch(gx_control_status(mode.valid,mode.live,connected.valid,connected.live,connected.valid?(int)connected.value:0,
        permission.valid,permission.live,permission.valid?(int)permission.value:0,mode.valid?(int)mode.value:0)) {
        case GxControlStatus::MissingMode:return "VE.Bus Mode fehlt: Instanz/Keepalive pruefen";
        case GxControlStatus::MissingConnection:return "GX-Rueckmeldung Connected fehlt";
        case GxControlStatus::Disconnected:return "GX meldet: MultiPlus getrennt (Connected=0)";
        case GxControlStatus::MissingPermission:return "GX-Rueckmeldung ModeIsAdjustable fehlt";
        case GxControlStatus::ReadOnly:return "GX-Fernsteuerung nicht freigegeben";
        case GxControlStatus::OffRemote:return "Aus: Einschalten ueber GX moeglich";
        default:return "MQTT verbunden";
    }
}
