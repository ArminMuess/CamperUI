#include "maxxfan_client.h"
#include "system_state.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

namespace {
portMUX_TYPE guard = portMUX_INITIALIZER_UNLOCKED;
FanStatus current;
struct Request { FanAction action; int value; bool demo; };
Request requested;
bool queued = false;
uint32_t last_poll = 0;
void store(const FanStatus &s) {
    portENTER_CRITICAL(&guard); current = s; portEXIT_CRITICAL(&guard);
}
void message(FanStatus &s, const char *text) { snprintf(s.message, sizeof(s.message), "%s", text); }
bool boolean(JsonVariant v, bool &out) {
    if (v.is<bool>()) { out = v.as<bool>(); return true; }
    String t = v.as<String>(); t.toLowerCase();
    if (t == "true" || t == "1") { out = true; return true; }
    if (t == "false" || t == "0") { out = false; return true; }
    return false;
}
bool number(JsonVariant v, int low, int high, int &out) {
    if (v.isNull()) return false;
    String t = v.as<String>(); t.replace(',', '.');
    char *end; float f = strtof(t.c_str(), &end);
    if (end == t.c_str() || *end || !isfinite(f) || f < low || f > high || floorf(f) != f) return false;
    out = (int)f; return true;
}
bool fetch(FanStatus &s) {
    String host;
    bool debug;
    {
        StateLockGuard lock;
        host = state.vanpi_ip;
        debug = state.debug_mode;
    }
    if (debug || WiFi.status() != WL_CONNECTED || host.length() < 7) return false;
    HTTPClient http;
    http.begin("http://" + host + ":1880/maxxfan/");
    http.setConnectTimeout(300); http.setTimeout(900);
    int code = http.GET();
    if (code != 200) {
        snprintf(s.message, sizeof(s.message), "MaxxFan: HTTP %d", code); http.end(); return false;
    }
    JsonDocument doc;
    auto error = deserializeJson(doc, http.getString()); http.end();
    JsonObject fan = doc["maxxfan"];
    String direction = fan["fan_direction"].as<String>();
    String lid = fan["fan_vent"].as<String>();
    if (error || !boolean(fan["fan_power"], s.power) || !boolean(fan["fan_auto"], s.automatic) ||
        !number(fan["fan_speed"], 1, 10, s.speed) || !number(fan["fan_temp"], -2, 37, s.target) ||
        (direction != "in" && direction != "out") || (lid != "open" && lid != "close")) {
        message(s, "MaxxFan: ungueltige Daten"); return false;
    }
    s.intake = direction == "in"; s.lid_open = lid == "open";
    s.valid = true; s.demo = false; s.updated = millis(); return true;
}
bool put(const String &path, FanStatus &s) {
    String host;
    bool debug;
    {
        StateLockGuard lock;
        host = state.vanpi_ip;
        debug = state.debug_mode;
    }
    if (debug || WiFi.status() != WL_CONNECTED) return false;
    HTTPClient http;
    http.begin("http://" + host + ":1880/maxxfan/" + path);
    http.setConnectTimeout(300); http.setTimeout(1200);
    int code = http.PUT(""); http.end();
    if (code != 200) {
        snprintf(s.message, sizeof(s.message), "Befehl: HTTP %d - Zustand pruefen", code); return false;
    }
    return true;
}
bool matches(const FanStatus &s, const Request &r) {
    switch (r.action) {
        case FanAction::Mode: return r.value == 0 ? !s.power && !s.automatic && !s.lid_open :
            r.value == 1 ? s.power && !s.automatic : s.automatic;
        case FanAction::Speed: return s.speed == r.value;
        case FanAction::Temperature: return s.target == r.value;
        case FanAction::Lid: return s.lid_open == (bool)r.value;
        case FanAction::Direction: return s.intake == (bool)r.value;
    }
    return false;
}
// Wait for acknowledgements across worker iterations, keeping other data polls alive.
Request active;
bool in_flight = false, waiting_auto = false, waiting_power = false, waiting_vent = false;
bool expected_power = false;
uint32_t deadline = 0, next_check = 0;
String active_host;
void finish(FanStatus &s, const char *text) {
    in_flight = false; s.pending = false; message(s, text); store(s); last_poll = millis();
}
void issue(FanStatus &s) {
    bool debug;
    String host;
    {
        StateLockGuard lock;
        debug = state.debug_mode;
        host = state.vanpi_ip;
    }
    if (debug != active.demo || host != active_host) { finish(s, "Datenquelle gewechselt"); return; }
    if (matches(s, active)) { finish(s, "Live - VanPi-Zustand bestaetigt"); return; }
    String path;
    waiting_auto = false; waiting_power = false; waiting_vent = false;
    if (active.action == FanAction::Mode) {
        bool automatic = active.value == 2;
        // VanPi auto is enable-only. Power leaves Auto and stops the fan.
        if (automatic && !s.automatic) { path = "auto"; waiting_auto = true; }
        else if (!automatic && s.automatic) {
            path = "power"; waiting_power = true; expected_power = false;
        } else if (!automatic && s.power != (active.value != 0)) {
            path = "power"; waiting_power = true; expected_power = active.value != 0;
        }
        else if (active.value == 0 && s.lid_open) { path = "vent"; waiting_vent = true; }
    } else {
        if (active.action == FanAction::Speed && s.power && !s.automatic) path = "speed/" + String(active.value);
        if (active.action == FanAction::Temperature && s.automatic) path = "temp/" + String(active.value);
        if (active.action == FanAction::Lid && !s.automatic) path = "vent";
        if (active.action == FanAction::Direction && !s.automatic) path = "direction";
    }
    if (!path.length()) { finish(s, "Modus geaendert - Zustand pruefen"); return; }
    if (!put(path, s)) { in_flight = false; s.pending = false; store(s); return; }
    // No repeated toggles after timeout or uncertain HTTP acknowledgement.
    deadline = millis() + 10000; next_check = millis() + 300;
    s.pending = true; message(s, "Warte auf Rueckmeldung..."); store(s);
}

}
FanStatus maxxfan_status() {
    portENTER_CRITICAL(&guard); FanStatus s = current; portEXIT_CRITICAL(&guard);
    bool debug;
    bool wifi_conn;
    {
        StateLockGuard lock;
        debug = state.debug_mode;
        wifi_conn = state.wifi_connected;
    }
    if (s.demo != debug || (!s.demo && (!wifi_conn || millis() - s.updated > 15000))) s.valid = false;
    return s;
}
bool maxxfan_request(FanAction action, int value) {
    FanStatus s = maxxfan_status();
    if (!s.valid || s.pending) return false;
    if ((action == FanAction::Mode && (value < 0 || value > 2)) ||
        (action == FanAction::Speed && (value < 1 || value > 10 || !s.power || s.automatic)) ||
        (action == FanAction::Temperature && (value < -2 || value > 37 || !s.automatic)) ||
        ((action == FanAction::Lid || action == FanAction::Direction) && (value < 0 || value > 1)) ||
        ((action == FanAction::Lid || action == FanAction::Direction) && s.automatic)) return false;
    bool debug;
    {
        StateLockGuard lock;
        debug = state.debug_mode;
    }
    portENTER_CRITICAL(&guard);
    if (current.pending || queued) { portEXIT_CRITICAL(&guard); return false; }
    requested = {action, value, debug}; queued = true; current.pending = true;
    snprintf(current.message, sizeof(current.message), "Warte auf Rueckmeldung...");
    portEXIT_CRITICAL(&guard); return true;
}
void maxxfan_worker() {
    FanStatus s = maxxfan_status();
    Request r{}; bool work;
    portENTER_CRITICAL(&guard); work = queued; if (work) r = requested; queued = false; portEXIT_CRITICAL(&guard);
    bool debug;
    String host;
    bool fan_enabled;
    {
        StateLockGuard lock;
        debug = state.debug_mode;
        host = state.vanpi_ip;
        fan_enabled = state.tab_enabled[TAB_MAXXFAN];
    }
    if (s.demo != debug) {
        in_flight = false; s = FanStatus{}; s.demo = debug;
        if (s.demo) { s.valid = true; s.updated = millis(); message(s, "Dummy - keine Schaltbefehle"); }
        store(s); last_poll = 0;
    }
    if (work && r.demo == debug) {
        if (r.demo) {
            switch (r.action) {
                case FanAction::Mode: s.automatic = r.value == 2; s.power = r.value != 0; s.lid_open = s.power; break;
                case FanAction::Speed: s.speed = r.value; break;
                case FanAction::Temperature: s.target = r.value; break;
                case FanAction::Lid: s.lid_open = r.value; break;
                case FanAction::Direction: s.intake = r.value; break;
            }
            s.valid = true; s.updated = millis(); s.pending = false;
            message(s, "Dummy - keine Schaltbefehle"); store(s);
        } else if (fetch(s)) {
            active = r; active_host = host; in_flight = true; issue(s);
        } else { s.valid = false; s.pending = false; store(s); }
        last_poll = millis();
    } else if (in_flight) {
        if (active.demo != debug || active_host != host) { finish(s, "Datenquelle gewechselt"); return; }
        if ((int32_t)(millis() - deadline) >= 0) { finish(s, waiting_auto ? "Auto nicht bestaetigt - VanPi pruefen" :
            waiting_power ? "Luefter aus/an nicht bestaetigt" :
            waiting_vent ? "Haube zu nicht bestaetigt" : "Keine Bestaetigung - Zustand pruefen"); return; }
        if ((int32_t)(millis() - next_check) >= 0) {
            if (fetch(s)) {
                s.pending = true; store(s);
                bool acknowledged = waiting_auto ? s.automatic == (active.value == 2) :
                    waiting_power ? s.power == expected_power && !s.automatic :
                    waiting_vent ? !s.lid_open : matches(s, active);
                if (acknowledged) issue(s);
            }
            next_check = millis() + 300;
        }
    } else if (!debug && fan_enabled && millis() - last_poll >= 2500) {
        s.pending = false;
        if (!fetch(s)) s.valid = false;
        else if (strncmp(s.message, "MaxxFan:", 8) == 0 || strcmp(s.message, "Warte auf VanPi") == 0) message(s, "Live - VanPi-Daten");
        store(s); last_poll = millis();
    } else if (work) { s.pending = false; store(s); }
}
