#include "victron_settings.h"
#include "victron_client.h"
#include "ui_main.h"

namespace {
lv_obj_t *root,*enabled,*keepalive,*show_tab,*starter_source,*battery_instance,*host,*port,*portal,*instance,*user,*password,*relay_names[2],*status;
String text(lv_obj_t *area){return lv_textarea_get_text(area);}
lv_obj_t *field(lv_obj_t *parent,const char *name,const char *value,int y,int maxlen,lv_event_cb_t callback) {
    lv_obj_t *l=lv_label_create(parent);lv_label_set_text(l,name);lv_obj_set_pos(l,0,y);
    lv_obj_set_style_text_font(l,&lv_font_montserrat_14,0);
    lv_obj_t *area=lv_textarea_create(parent);lv_textarea_set_one_line(area,true);
    lv_textarea_set_max_length(area,maxlen);lv_textarea_set_text(area,value);
    lv_obj_set_pos(area,0,y+24);lv_obj_set_size(area,426,44);
    lv_obj_set_style_text_font(area,&lv_font_montserrat_14,0);
    lv_obj_add_event_cb(area,callback,LV_EVENT_ALL,nullptr);return area;
}
lv_obj_t *toggle(lv_obj_t *parent,const char *caption,bool checked,int y) {
    lv_obj_t *s=lv_switch_create(parent);lv_obj_set_pos(s,0,y);lv_obj_set_size(s,50,28);
    if(checked)lv_obj_add_state(s,LV_STATE_CHECKED);
    lv_obj_t *l=lv_label_create(parent);lv_label_set_text(l,caption);lv_obj_set_pos(l,65,y+5);
    lv_obj_set_style_text_font(l,&lv_font_montserrat_14,0);return s;
}
bool gx_read_bool(JsonVariantConst root,const char *key,bool &target,String &error) {
    if(!root.containsKey(key))return true;
    if(!root[key].is<bool>()){error=String(key)+": EIN/AUS erwartet";return false;}
    target=root[key].as<bool>();return true;
}
bool string_value(JsonVariantConst root,const char *key,String &target,size_t max,String &error) {
    if(!root.containsKey(key))return true;
    if(!root[key].is<const char*>()){error=String(key)+": Text erwartet";return false;}
    target=root[key].as<String>();
    if(target.length()>max){error=String(key)+": Text zu lang";return false;}return true;
}
}
void victron_fill_settings(JsonObject doc) {
    StateLockGuard lock;
    doc["victron_enabled"]=state.victron_enabled;doc["victron_keepalive"]=state.victron_keepalive;
    doc["victron_starter"]=state.victron_starter;doc["victron_battery_instance"]=state.victron_battery_instance;
    doc["victron_host"]=state.victron_host;doc["victron_port"]=state.victron_port;
    doc["victron_portal"]=state.victron_portal;doc["victron_instance"]=state.victron_instance;
    doc["victron_user"]=state.victron_user;doc["victron_password_set"]=state.victron_password.length()>0;
    doc["home_show_victron"]=state.home_show_victron;
    JsonArray names=doc["victron_relay_names"].to<JsonArray>();
    JsonArray favorites=doc["victron_relays"].to<JsonArray>();
    for(int i=0;i<2;++i){names.add(state.victron_relay_names[i]);JsonObject item=favorites.add<JsonObject>();
        item["id"]=17+i;item["name"]=state.victron_relay_names[i];}
}
bool victron_apply_settings(JsonVariantConst doc,String &error) {
    StateLockGuard lock;
    bool en=state.victron_enabled,ka=state.victron_keepalive,starter=state.victron_starter;int battery_id=state.victron_battery_instance;
    String h=state.victron_host,pid=state.victron_portal,u=state.victron_user,pw=state.victron_password;
    String names[2]={state.victron_relay_names[0],state.victron_relay_names[1]};
    int mqtt_port=state.victron_port,id=state.victron_instance;
    if(!gx_read_bool(doc,"victron_enabled",en,error)||!gx_read_bool(doc,"victron_keepalive",ka,error)||!gx_read_bool(doc,"victron_starter",starter,error)||
       !string_value(doc,"victron_host",h,63,error)||!string_value(doc,"victron_portal",pid,32,error)||
       !string_value(doc,"victron_user",u,64,error))return false;
    h.trim();pid.trim();
    if(!gx_identifier(h.c_str(),63,true)||!gx_identifier(pid.c_str(),32)) {
        error="Cerbo-Adresse/Portal-ID ungueltig: keine Leerzeichen, /, + oder #";return false;
    }
    if(doc.containsKey("victron_port")) {
        if(!doc["victron_port"].is<int>()){error="MQTT-Port muss eine Zahl sein";return false;}
        mqtt_port=doc["victron_port"].as<int>();
    }
    if(doc.containsKey("victron_instance")) {
        if(!doc["victron_instance"].is<int>()){error="Geraete-ID muss eine Zahl sein (-1 = Auto)";return false;}
        id=doc["victron_instance"].as<int>();
    }
    if(mqtt_port<1||mqtt_port>65535||id<-1||id>65535){error="MQTT-Port oder Geraete-ID ausserhalb des Bereichs";return false;}
    if(doc.containsKey("victron_battery_instance")) {
        if(!doc["victron_battery_instance"].is<int>()){error="Batteriemonitor-ID muss eine Zahl sein";return false;}
        battery_id=doc["victron_battery_instance"].as<int>();
        if(battery_id<-1||battery_id>65535){error="Batteriemonitor-ID ausserhalb des Bereichs";return false;}
    }
    bool clear=false;if(!gx_read_bool(doc,"victron_clear_password",clear,error))return false;
    if(clear)pw="";
    if(doc.containsKey("victron_password")) {
        String entered;
        if(!string_value(doc,"victron_password",entered,64,error))return false;
        if(entered.length())pw=entered; // blank Web field preserves stored password
    }
    if(doc.containsKey("victron_relay_names")) {
        if(!doc["victron_relay_names"].is<JsonArrayConst>()||doc["victron_relay_names"].size()!=2) {
            error="Bitte zwei GX-Relaisnamen angeben";return false;
        }
        for(int i=0;i<2;++i) {
            if(!doc["victron_relay_names"][i].is<const char*>()){error="Relaisname muss Text sein";return false;}
            names[i]=doc["victron_relay_names"][i].as<String>();names[i].trim();
            names[i].replace("\n"," ");names[i].replace("\r"," ");
            if(names[i].length()>32){error="GX-Relaisname: maximal 32 Zeichen";return false;}
            if(!names[i].length())names[i]=String("GX Relais ")+String(i+1);
        }
    }
    state.victron_starter=starter;state.victron_battery_instance=battery_id;
    state.victron_enabled=en;state.victron_keepalive=ka;state.victron_host=h;state.victron_portal=pid;
    state.victron_port=mqtt_port;state.victron_instance=id;state.victron_user=u;state.victron_password=pw;
    for(int i=0;i<2;++i)state.victron_relay_names[i]=names[i];return true;
}
void ui_victron_settings_reset(){root=enabled=keepalive=show_tab=starter_source=battery_instance=host=port=portal=instance=user=password=status=nullptr;relay_names[0]=relay_names[1]=nullptr;}
void ui_victron_settings_build(lv_obj_t *parent,lv_event_cb_t callback) {
    root=parent;lv_obj_add_flag(parent,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(parent,LV_DIR_VER);
    lv_obj_set_style_pad_bottom(parent,90,0);
    lv_obj_t *title=lv_label_create(parent);lv_label_set_text(title,"Victron GX");lv_obj_set_pos(title,0,0);
    lv_obj_set_style_text_font(title,&lv_font_montserrat_18,0);lv_obj_set_style_text_color(title,lv_color_hex(UI_COLOR_PRIMARY),0);
    enabled=toggle(parent,"MQTT-Anbindung",state.victron_enabled,35);
    keepalive=toggle(parent,"Eigener Victron-Keepalive",state.victron_keepalive,75);
    show_tab=toggle(parent,"Victron-Tab anzeigen",state.tab_enabled[TAB_VICTRON],115);
    status=lv_label_create(parent);lv_obj_set_pos(status,0,157);lv_obj_set_width(status,426);
    lv_label_set_long_mode(status,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_font(status,&lv_font_montserrat_12,0);
    host=field(parent,"Cerbo-IP / Hostname",state.victron_host.c_str(),222,63,callback);
    lv_textarea_set_placeholder_text(host,"z. B. 192.168.1.50");
    port=field(parent,"MQTT-Port (TCP, ohne TLS)",String(state.victron_port).c_str(),300,5,callback);
    lv_textarea_set_accepted_chars(port,"0123456789");
    portal=field(parent,"Portal-ID (leer = automatisch)",state.victron_portal.c_str(),378,32,callback);
    instance=field(parent,"VE.Bus-Instanz (leer = automatisch)",state.victron_instance<0?"":String(state.victron_instance).c_str(),456,5,callback);
    lv_textarea_set_accepted_chars(instance,"0123456789");
    relay_names[0]=field(parent,"Relais 1: Anzeigename",state.victron_relay_names[0].c_str(),534,32,callback);
    relay_names[1]=field(parent,"Relais 2: Anzeigename",state.victron_relay_names[1].c_str(),612,32,callback);
    user=field(parent,"MQTT-Benutzer (optional)",state.victron_user.c_str(),690,64,callback);
    password=field(parent,"MQTT-Passwort (optional)",state.victron_password.c_str(),768,64,callback);
    lv_textarea_set_password_mode(password,true);
    starter_source=toggle(parent,"Starterspannung vom Cerbo GX",state.victron_starter,850);
    battery_instance=field(parent,"Batteriemonitor-ID (leer = automatisch)",state.victron_battery_instance<0?"":String(state.victron_battery_instance).c_str(),890,5,callback);
    lv_textarea_set_accepted_chars(battery_instance,"0123456789");
    lv_obj_t *hint=lv_label_create(parent);lv_obj_set_pos(hint,0,978);lv_obj_set_width(hint,426);
    lv_obj_set_style_text_font(hint,&lv_font_montserrat_12,0);lv_obj_set_style_text_color(hint,ui_theme_muted(),0);
    lv_label_set_text(hint,"Keepalive AUS, wenn VanPi ihn bereits sendet.\nOhne Daten: Portal-ID eintragen oder Keepalive pruefen.\nGX-Relais am Cerbo auf Manuell stellen.\nVE.Bus: VRM-Instanz, nicht Product-ID.\nStarter: SmartShunt/BMV mit Starter-Eingang.\nDummy-Modus: Simulation, keine MQTT-Schaltbefehle.");
    ui_victron_settings_update();
}
bool ui_victron_settings_apply(String &error) {
    if(!root)return true;
    JsonDocument doc;
    doc["victron_enabled"]=lv_obj_has_state(enabled,LV_STATE_CHECKED);
    doc["victron_keepalive"]=lv_obj_has_state(keepalive,LV_STATE_CHECKED);
    doc["victron_starter"]=lv_obj_has_state(starter_source,LV_STATE_CHECKED);
    doc["victron_battery_instance"]=text(battery_instance).length()?text(battery_instance).toInt():-1;
    doc["victron_host"]=text(host);doc["victron_port"]=text(port).toInt();
    doc["victron_portal"]=text(portal);doc["victron_instance"]=text(instance).length()?text(instance).toInt():-1;
    doc["victron_user"]=text(user);doc["victron_password"]=text(password);
    doc["victron_clear_password"]=text(password).length()==0;
    JsonArray names=doc["victron_relay_names"].to<JsonArray>();for(auto area:relay_names)names.add(text(area));
    if(!victron_apply_settings(doc.as<JsonVariantConst>(),error))return false;
    state.tab_enabled[TAB_VICTRON]=lv_obj_has_state(show_tab,LV_STATE_CHECKED);
    return true;
}
void ui_victron_settings_update() {
    if(!root||!status)return;
    VictronSnapshot s=victron_snapshot();char line[220];
    snprintf(line,sizeof(line),"%s\nPortal: %s | VE.Bus: %s%d",victron_status(s),s.portal[0]?s.portal:"Auto / warte",s.instance<0?"Auto / ":"",s.instance);
    String diagnosis=line;
    if(state.victron_starter) {
        diagnosis+="\nStarter GX: ";
        if(s.starter_ambiguous)diagnosis+="mehrere Monitore: ID waehlen";
        else if(victron_fresh(s,GX_STARTER))diagnosis+=String(s.values[GX_STARTER].value,1)+" V";
        else diagnosis+="keine aktuellen Daten";
    }
    ui_label_set_text_if_changed(status,diagnosis.c_str());
}
