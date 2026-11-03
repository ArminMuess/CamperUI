#include "volumio_client.h"
#include "radio_config.h"
#include "system_state.h"
#include "config/volumio_config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
namespace {
portMUX_TYPE guard = portMUX_INITIALIZER_UNLOCKED;
RadioSnapshot current;
bool busy = false;
struct Request { radio_protocol::Action action; int value; uint32_t created, generation; };
QueueHandle_t queue = nullptr;
TaskHandle_t task = nullptr;
void publish(const RadioSnapshot &s) {
    if(s.generation != radio_endpoint().generation)return;
    portENTER_CRITICAL(&guard); current = s; current.pending = busy; portEXIT_CRITICAL(&guard);
}
void message(RadioSnapshot &s, const char *text) {
    snprintf(s.message, sizeof(s.message), "%s", text);
}
String url(const RadioEndpoint &e,const char *path) {
    return String("http://") + e.config.host + ":" + String(e.config.port) + path;
}
void prepare(HTTPClient &http) {
    http.setConnectTimeout(350);
    http.setTimeout(750);
    http.setReuse(false);
    http.useHTTP10(true); // getStream() must not receive chunk framing.
}
// Only a fully parsed getState response is published as a fresh player state.
bool fetch(RadioSnapshot &s, const RadioEndpoint &e) {
    HTTPClient http; prepare(http);
    if (!http.begin(url(e,"/api/v1/getState"))) { message(s, "Volumio: URL ungueltig"); return false; }
    int code = http.GET();
    if (code != 200) {
        snprintf(s.message, sizeof(s.message), "Volumio: HTTP %d", code); http.end(); return false;
    }
    // Bound metadata allocation; ignore album art / unrelated state fields.
    JsonDocument filter;
    filter["status"] = true; filter["volume"] = true;
    filter["disableVolumeControl"] = true; filter["mute"] = true;
    filter["artist"] = true; filter["title"] = true;
    filter["service"] = true; filter["uri"] = true;
    JsonDocument doc;
    auto error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    const char *status = doc["status"] | "";
    if (error || (strcmp(status, "play") && strcmp(status, "pause") && strcmp(status, "stop"))) {
        message(s, "Volumio: ungueltiger Playerstatus"); return false;
    }
    snprintf(s.playback, sizeof(s.playback), "%s", status);
    snprintf(s.artist, sizeof(s.artist), "%s", doc["artist"] | "");
    snprintf(s.title, sizeof(s.title), "%s", doc["title"] | "");
    bool numeric = doc["volume"].is<int>() || doc["volume"].is<float>();
    float volume = numeric ? doc["volume"].as<float>() : -1;
    s.volume_available = numeric && isfinite(volume) && volume >= 0 && volume <= 100 &&
        !(doc["disableVolumeControl"] | false);
    if (s.volume_available) s.volume = int(lroundf(volume));
    snprintf(s.service,sizeof(s.service),"%s",doc["service"] | "");
    snprintf(s.uri,sizeof(s.uri),"%s",doc["uri"] | "");
    s.muted = doc["mute"] | false;
    s.online = true; s.updated = millis(); return true;
}
bool send(const Request &r, RadioSnapshot &s, const RadioEndpoint &e) {
    const char *playlist=r.action==radio_protocol::Action::Preset?e.config.playlists[r.value]:"";
    const auto path=radio_protocol::path(r.action,r.value,playlist);
    if(path.empty()){message(s,"Radio: ungueltiger Befehl");return false;}
    HTTPClient http;prepare(http);
    if(!http.begin(url(e,path.c_str()))){message(s,"Radio: URL ungueltig");return false;}
    int code;
    if(r.action==radio_protocol::Action::DabPreset) {
        if(!*e.config.dab_uris[r.value]){http.end();message(s,"DAB-Preset noch nicht eingerichtet");return false;}
        JsonDocument body;
        body["item"]["uri"]=e.config.dab_uris[r.value];
        body["item"]["service"]="rtlsdr_radio";
        body["item"]["type"]="webradio";
        body["item"]["title"]=radio_config::PRESET_LABELS[r.value];
        String payload;serializeJson(body,payload);
        http.addHeader("Content-Type","application/json");code=http.POST(payload);
    } else code=http.GET();
    http.end();
    if(code!=200){snprintf(s.message,sizeof(s.message),"Befehl HTTP %d - Zustand pruefen",code);return false;}
    // Success of the request is not confirmation of audible playback.
    return true;
}
bool demo_mode(){StateLockGuard lock;return state.debug_mode;}
void worker(void *) {
    RadioSnapshot s;
    uint32_t last_poll=millis()-radio_config::POLL_MS;
    char command_error[128]="";
    for(;;) {
        auto e=radio_endpoint();
        if(s.generation!=e.generation) {
            s=RadioSnapshot();s.generation=e.generation;
            command_error[0]=0;last_poll=millis()-radio_config::POLL_MS;
        }
        s.configured=e.config.enabled && *e.config.host && !demo_mode();
        Request r;
        if(xQueueReceive(queue,&r,pdMS_TO_TICKS(40))==pdTRUE) {
            if(!s.configured || WiFi.status()!=WL_CONNECTED ||
               !radio_protocol::current_command(millis(),r.created,radio_config::COMMAND_TTL_MS,r.generation,e.generation)) {
                snprintf(command_error,sizeof(command_error),"Befehl verworfen - erneut bedienen");
            } else {
                // Recheck endpoint immediately before dispatch. In-flight HTTP cannot be undone.
                if(demo_mode() || radio_endpoint().generation!=e.generation) {
                    snprintf(command_error,sizeof(command_error),"Adresse geaendert - Befehl verworfen");
                } else if(send(r,s,e)) command_error[0]=0;
                else snprintf(command_error,sizeof(command_error),"%s",s.message);
            }
            if(s.configured && WiFi.status()==WL_CONNECTED)s.online=fetch(s,e);
            else s.online=false;
            if(s.online)message(s,*command_error?command_error:"Volumio verbunden");
            else if(*command_error)message(s,command_error);
            portENTER_CRITICAL(&guard);busy=false;portEXIT_CRITICAL(&guard);
            publish(s);last_poll=millis();
        }
        if(uint32_t(millis()-last_poll)>=radio_config::POLL_MS) {
            if(!s.configured){s.online=false;message(s,demo_mode()?"CamperUI-Demomodus: Radio deaktiviert":e.config.enabled?"Volumio-Adresse unter Zahnrad eintragen":"Radio deaktiviert");}
            else if(WiFi.status()!=WL_CONNECTED){s.online=false;message(s,"WLAN getrennt");}
            else {
                s.online=fetch(s,e);
                if(s.online)message(s,*command_error?command_error:"Volumio verbunden");
            }
            publish(s);last_poll=millis();
        }
    }
}
}
bool radio_init() {
    if (task) return true;
    radio_config_load();
    current.generation=radio_endpoint().generation;
    queue = xQueueCreate(1, sizeof(Request));
    if (!queue) { message(current, "Radio: kein Speicher fuer Warteschlange"); return false; }
    // Separate low-priority worker. No LVGL access from this task.
    if (xTaskCreate(worker, "volumio", 8192, nullptr, 1, &task) != pdPASS) {
        vQueueDelete(queue); queue = nullptr;
        message(current, "Radio: Worker konnte nicht starten"); return false;
    }
    return true;
}
RadioSnapshot radio_snapshot() {
    portENTER_CRITICAL(&guard); RadioSnapshot s = current; portEXIT_CRITICAL(&guard);
    auto e=radio_endpoint();
    if(s.generation!=e.generation){s=RadioSnapshot();s.generation=e.generation;
        s.configured=e.config.enabled && *e.config.host && !demo_mode();
        message(s,"Radio-Einstellungen werden uebernommen");}
    if(demo_mode()){s.online=false;message(s,"CamperUI-Demomodus: Radio deaktiviert");}
    if (s.online && uint32_t(millis() - s.updated) >= radio_config::STALE_MS) s.online = false;
    return s;
}
bool radio_request(radio_protocol::Action action, int value) {
    if (demo_mode() || !radio_protocol::valid(action, value) || !queue || WiFi.status() != WL_CONNECTED) return false;
    auto e=radio_endpoint();
    portENTER_CRITICAL(&guard);
    bool ready = e.config.enabled && current.generation==e.generation && current.online && !busy &&
        uint32_t(millis() - current.updated) < radio_config::STALE_MS &&
        (action != radio_protocol::Action::Volume || current.volume_available);
    if (ready) { busy = true; current.pending = true; }
    portEXIT_CRITICAL(&guard);
    if (!ready) return false;
    Request r{action, value, millis(), e.generation};
    if (xQueueSend(queue, &r, 0) == pdTRUE) return true;
    portENTER_CRITICAL(&guard); busy = false; current.pending = false; portEXIT_CRITICAL(&guard);
    return false;
}
