#include "http_handler.h"
#include "ui_main.h"
#include "ui_screensaver.h"
#include "maxxfan_client.h"
#include "relay_click_filter.h"
#include <math.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "system_state.h"
#include "HWCDC.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "debug_log.h"

struct HttpCmd {
    char path[64];
};
static QueueHandle_t http_cmd_queue = NULL;

static void queue_cmd(const char* path) {
    if (state.debug_mode || !http_cmd_queue) return;
    HttpCmd cmd;
    strncpy(cmd.path, path, sizeof(cmd.path) - 1);
    cmd.path[sizeof(cmd.path) - 1] = '\0';
    xQueueSend(http_cmd_queue, &cmd, 0);
}

unsigned long last_http_poll = 0;
static int poll_step = 0;

// Replace UTF-8 encoded German umlauts with ASCII equivalents
// German umlauts are fully supported by our LVGL font fallbacks.
static String fix_umlauts(const char *in) {
    if (!in) return String();
    return String(in);
}

static String fix_umlauts(const String &in) {
    return fix_umlauts(in.c_str());
}

static String get_url() {
    StateLockGuard lock;
    return "http://" + state.vanpi_ip + ":1880";
}

// Node-RED globals may be stored as strings or numbers.
// These helpers handle both: "13.34" (string) and 13.34 (number), including German comma format.
static float jsonFloat(JsonVariant v) {
    if (v.isNull()) return 0.0f;
    if (v.is<float>()) return v.as<float>();
    if (v.is<int>()) return (float)v.as<int>();
    const char *s = v.as<const char*>();
    if (!s) return 0.0f;
    char buf[24];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char *p = buf; *p; ++p) {
        if (*p == ',') *p = '.';
    }
    return atof(buf);
}

// Energy histories must not treat missing/null/text responses as fresh zeroes.
static bool validEnergyNumber(JsonVariant v) {
    if (v.isNull() || v.is<bool>()) return false;
    if (v.is<float>() || v.is<int>()) return isfinite(v.as<float>());
    const char *s=v.as<const char*>();
    if (!s || !*s || strlen(s)>=24) return false;
    char buf[24]; strcpy(buf,s);
    for (char *p=buf;*p;++p) if (*p==',') *p='.';
    char *end; float value=strtof(buf,&end);
    if (end==buf) return false;
    while (*end==' ' || *end=='\t' || *end=='\r' || *end=='\n') ++end;
    return *end=='\0' && isfinite(value);
}

static int jsonInt(JsonVariant v) {
    if (v.isNull()) return 0;
    if (v.is<int>()) return v.as<int>();
    const char *s = v.as<const char*>();
    if (!s) return 0;
    return atoi(s);
}

static bool jsonBool(JsonVariant v) {
    if (v.isNull()) return false;
    if (v.is<bool>()) return v.as<bool>();
    if (v.is<int>()) return v.as<int>() != 0;
    const char *s = v.as<const char*>();
    if (!s) return false;
    return (strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcasecmp(s, "on") == 0 || strcasecmp(s, "yes") == 0);
}

void http_init() {
    if (!http_cmd_queue) {
        http_cmd_queue = xQueueCreate(16, sizeof(HttpCmd));
    }
}

void http_fetch_names() {
    // Names come embedded in /relay and /dimmer responses
}

// Static lookup tables to prevent String heap allocations in parsing loops
static const char* const RELAY_KEYS[16]      = {
    "Relay1", "Relay2", "Relay3", "Relay4", "Relay5", "Relay6", "Relay7", "Relay8",
    "Relay9", "Relay10", "Relay11", "Relay12", "Relay13", "Relay14", "Relay15", "Relay16"
};
static const char* const WIFIRELAIS_KEYS[8]   = {"WifiRelay1", "WifiRelay2", "WifiRelay3", "WifiRelay4", "WifiRelay5", "WifiRelay6", "WifiRelay7", "WifiRelay8"};
static const char* const WRELAY_KEYS_ALT[8]   = {"wrelay1", "wrelay2", "wrelay3", "wrelay4", "wrelay5", "wrelay6", "wrelay7", "wrelay8"};
static const char* const DIMMER_KEYS[8]      = {"dimmer1", "dimmer2", "dimmer3", "dimmer4", "dimmer5", "dimmer6", "dimmer7", "dimmer8"};
static const char* const DIMMER_PRO_KEYS[10] = {
    "dimmer8", "dimmer9", "dimmer10", "dimmer11", "dimmer12", "dimmer13", "dimmer14", "dimmer15",
    "rgbw1", "rgbw2"
};
static const char* const LEVEL_KEYS[4]       = {"level1", "level2", "level3", "level4"};
static const char* const TEMP_KEYS[4]        = {"temp1", "temp2", "temp3", "temp4"};

// --- Parsers ---
// GET /batt -> { "VoltB": float|str, "Ampere": float|str, "battsoc": int|str }
static void parse_batt_json(const char *payload) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    if (validEnergyNumber(doc["VoltB"])) { state.bat_voltage = jsonFloat(doc["VoltB"]); state.battery_fields |= 1; state.bat_voltage_updated_ms = millis(); }
    if (validEnergyNumber(doc["Ampere"])) { state.bat_current = jsonFloat(doc["Ampere"]); state.battery_fields |= 2; state.bat_current_updated_ms = millis(); }
    if (validEnergyNumber(doc["battsoc"])) { state.bat_soc = jsonInt(doc["battsoc"]); state.battery_fields |= 4; state.bat_soc_updated_ms = millis(); }
    // Starter voltage / Lima
    if (doc.containsKey("starter_voltage")) {
        state.starter_voltage = jsonFloat(doc["starter_voltage"]);
        state.battery_fields |= 8;
    } else if (doc.containsKey("VoltS")) {
        state.starter_voltage = jsonFloat(doc["VoltS"]);
        state.battery_fields |= 8;
    } else if (doc.containsKey("starter")) {
        state.starter_voltage = jsonFloat(doc["starter"]);
        state.battery_fields |= 8;
    }

}

// GET /shore -> { "connected": bool, "power": float, "voltage": float, "current": float }
static void parse_shore_json(const char *payload) {
    if (!payload || payload[0] == '\0') return;
    StaticJsonDocument<384> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    state.shore_available = true;

    bool conn = false;
    if (doc.containsKey("connected")) conn = jsonBool(doc["connected"]);
    else if (doc.containsKey("shore_connected")) conn = jsonBool(doc["shore_connected"]);
    else if (doc.containsKey("state")) conn = jsonBool(doc["state"]);
    else if (doc.containsKey("on")) conn = jsonBool(doc["on"]);

    float p = 0.0f;
    if (validEnergyNumber(doc["power"])) p = jsonFloat(doc["power"]);
    else if (validEnergyNumber(doc["watt"])) p = jsonFloat(doc["watt"]);
    else if (validEnergyNumber(doc["shore_power"])) p = jsonFloat(doc["shore_power"]);

    float v = 0.0f;
    if (validEnergyNumber(doc["voltage"])) v = jsonFloat(doc["voltage"]);
    else if (validEnergyNumber(doc["volt"])) v = jsonFloat(doc["volt"]);
    else if (validEnergyNumber(doc["shore_voltage"])) v = jsonFloat(doc["shore_voltage"]);

    float i = 0.0f;
    if (validEnergyNumber(doc["current"])) i = jsonFloat(doc["current"]);
    else if (validEnergyNumber(doc["amp"])) i = jsonFloat(doc["amp"]);
    else if (validEnergyNumber(doc["ampere"])) i = jsonFloat(doc["ampere"]);
    else if (validEnergyNumber(doc["shore_current"])) i = jsonFloat(doc["shore_current"]);

    if (p > 5.0f) conn = true;
    if (conn && v < 1.0f) v = 230.0f;
    if (conn && i < 0.01f && v > 50.0f && p > 0.0f) i = p / v;

    state.shore_connected = conn;
    state.shore_power = conn ? p : 0.0f;
    state.shore_voltage = conn ? v : 0.0f;
    state.shore_current = conn ? i : 0.0f;
}

static void on_shore_error() {
    StateLockGuard lock;
    state.shore_available = false;
    state.shore_connected = false;
    state.shore_power = 0.0f;
    state.shore_current = 0.0f;
    state.shore_voltage = 0.0f;
}

// GET /booster -> { "active": bool, "power": float, "voltage": float, "current": float }
static void parse_booster_json(const char *payload) {
    if (!payload || payload[0] == '\0') return;
    StaticJsonDocument<384> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    state.alt_available = true;

    bool act = false;
    if (doc.containsKey("active")) act = jsonBool(doc["active"]);
    else if (doc.containsKey("alt_active")) act = jsonBool(doc["alt_active"]);
    else if (doc.containsKey("state")) act = jsonBool(doc["state"]);
    else if (doc.containsKey("on")) act = jsonBool(doc["on"]);

    float p = 0.0f;
    if (validEnergyNumber(doc["power"])) p = jsonFloat(doc["power"]);
    else if (validEnergyNumber(doc["watt"])) p = jsonFloat(doc["watt"]);
    else if (validEnergyNumber(doc["alt_power"])) p = jsonFloat(doc["alt_power"]);
    else if (validEnergyNumber(doc["b2b_power"])) p = jsonFloat(doc["b2b_power"]);

    float v = 0.0f;
    if (validEnergyNumber(doc["voltage"])) v = jsonFloat(doc["voltage"]);
    else if (validEnergyNumber(doc["volt"])) v = jsonFloat(doc["volt"]);
    else if (validEnergyNumber(doc["starter_voltage"])) v = jsonFloat(doc["starter_voltage"]);

    float i = 0.0f;
    if (validEnergyNumber(doc["current"])) i = jsonFloat(doc["current"]);
    else if (validEnergyNumber(doc["amp"])) i = jsonFloat(doc["amp"]);
    else if (validEnergyNumber(doc["ampere"])) i = jsonFloat(doc["ampere"]);
    else if (validEnergyNumber(doc["alt_current"])) i = jsonFloat(doc["alt_current"]);

    if (p > 5.0f) act = true;
    if (v > 1.0f) state.starter_voltage = v;

    state.alt_active = act;
    state.alt_power = act ? p : 0.0f;
    state.alt_current = act ? i : 0.0f;
}

static void on_booster_error() {
    StateLockGuard lock;
    state.alt_available = false;
    state.alt_active = false;
    state.alt_power = 0.0f;
    state.alt_current = 0.0f;
}

// GET /mppt/ -> { "mppt_pv_watts": float, "mppt_pv_amps": float, "mppt_pv_volts": float }
static void parse_mppt_json(const char *payload) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    if (validEnergyNumber(doc["mppt_pv_watts"])) { state.solar_power = jsonFloat(doc["mppt_pv_watts"]); state.solar_fields |= 1; state.solar_power_updated_ms = millis(); }
    if (doc.containsKey("mppt_pv_amps")) { state.solar_current = jsonFloat(doc["mppt_pv_amps"]); state.solar_fields |= 2; }
    if (doc.containsKey("mppt_pv_volts")) { state.solar_voltage = jsonFloat(doc["mppt_pv_volts"]); state.solar_fields |= 4; }
}

// GET /relay -> { "Relay1": { "state": bool, "name": str, ... }, "Relay2": ... }
static void parse_relay_json(const char *payload) {
    StaticJsonDocument<2048> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    int max_r = state.show_relay_ext ? 16 : 8;
    for (int i = 0; i < max_r; i++) {
        const char *key = RELAY_KEYS[i];
        if (!doc.containsKey(key)) continue;
        JsonObject r = doc[key];
        if (r.containsKey("state")) {
            if ((int32_t)(millis() - state.relay_hold_until[i]) >= 0) state.switch_state[i] = r["state"].as<bool>();
            state.relay_fields |= (1 << i);
        }
        if (r.containsKey("name")) {
            const char *n = r["name"].as<const char*>();
            if (n && n[0] != '\0' && strcmp(n, key) != 0) {
                String clean = fix_umlauts(n);
                if (state.switch_names[i] != clean) state.switch_names[i] = clean;
            }
        }
    }
}

// GET /wrelay -> { "WifiRelay1": { "state": bool|str, "name": str, ... }, ... }
static void parse_wrelay_json(const char *payload) {
    if (!payload || payload[0] == '\0') return;
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[WRELAY] deserializeJson error: %s\n", err.c_str());
        return;
    }

    StateLockGuard lock;
    for (int i = 0; i < 8; i++) {
        JsonObject r;
        if (doc.containsKey(WIFIRELAIS_KEYS[i])) {
            r = doc[WIFIRELAIS_KEYS[i]];
        } else if (doc.containsKey(WRELAY_KEYS_ALT[i])) {
            r = doc[WRELAY_KEYS_ALT[i]];
        } else {
            continue;
        }
        if (r.isNull()) continue;

        if (r.containsKey("state")) {
            if ((int32_t)(millis() - state.wrelay_hold_until[i]) >= 0) {
                bool val = false;
                if (r["state"].is<bool>()) {
                    val = r["state"].as<bool>();
                } else {
                    const char *s = r["state"].as<const char*>();
                    if (s) {
                        val = (strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcasecmp(s, "on") == 0);
                    }
                }
                state.wrelay_state[i] = val;
            }
            state.wrelay_fields |= (1 << i);
        }
        if (r.containsKey("name")) {
            const char *n = r["name"].as<const char*>();
            if (n && n[0] != '\0') {
                String clean = fix_umlauts(n);
                if (state.wrelay_names[i] != clean) {
                    state.wrelay_names[i] = clean;
                    Serial.printf("[WRELAY] Relay %d name updated: '%s'\n", i + 1, clean.c_str());
                }
            }
        }
    }
}


// GET /dimmer -> { "dimmer1": { "state": int(0-100), "name": str, ... }, "dimmer2": ... }
static void parse_dimmer_json(const char *payload) {
    StaticJsonDocument<2048> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    int max_dim = state.dimmy_pro ? 10 : 8;
    const char* const* keys = state.dimmy_pro ? DIMMER_PRO_KEYS : DIMMER_KEYS;
    for (int i = 0; i < max_dim; i++) {
        const char *key = keys[i];
        if (!doc.containsKey(key)) continue;
        JsonObject d = doc[key];
        if (d.containsKey("state") && (int32_t)(millis() - state.dimmer_hold_until[i]) >= 0) {
            int bri = 0;
            if (d["state"].is<JsonObject>()) {
                JsonObject st = d["state"].as<JsonObject>();
                if (st.containsKey("bri")) {
                    bri = jsonInt(st["bri"]);
                } else if (st.containsKey("white")) {
                    bri = jsonInt(st["white"]);
                }
            } else {
                bri = jsonInt(d["state"]);
            }
            state.dimmer_val[i] = constrain(bri, 0, 100);
            state.dimmer_fields |= (1 << i);
        }
        if (d.containsKey("name")) {
            const char *n = d["name"].as<const char*>();
            if (n && n[0] != '\0' && strcmp(n, key) != 0) {
                String clean = fix_umlauts(n);
                if (state.dimmer_names[i] != clean) state.dimmer_names[i] = clean;
            }
        }
    }
}

// GET /level -> { "level1": { "state": int, "name": str }, ... }
static void parse_level_json(const char *payload) {
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    for (int i = 0; i < 4; i++) {
        const char *key = LEVEL_KEYS[i];
        if (!doc.containsKey(key)) continue;
        JsonObject lvl = doc[key];
        if (lvl.containsKey("state")) { state.tank_level[i] = constrain(jsonInt(lvl["state"]), 0, 100); state.tank_fields |= (1 << i); }
        if (lvl.containsKey("name")) {
            const char *n = lvl["name"].as<const char*>();
            if (n && n[0] != '\0') {
                String clean = fix_umlauts(n);
                if (state.tank_names[i] != clean) state.tank_names[i] = clean;
            }
        }
    }
}

// GET /temp -> { "temp1": { "state": "23.5", "name": str }, "temp2": ... }
static void parse_temp_json(const char *payload) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    uint16_t fields = 0;
    for (int i = 0; i < TEMP_SOURCE_COUNT; i++) {
        String key = i < 4 ? "temp" + String(i + 1) : "ruuvitag" + String(i - 4);
        JsonObject t = doc[key];
        if (t.isNull()) continue;
        String n = t["name"] | ""; n.replace('\n', ' '); n.replace('\r', ' ');
        if (n.length()) {
            String name = fix_umlauts(n.c_str()).substring(0, 48);
            if (state.temp_sensor_names[i] != name) state.temp_sensor_names[i] = name;
        }
        state.temp_is_humidity[i] = state.debug_mode && i == 2;
        if (!t["state"].isNull()) {
            String raw = t["state"].as<String>(); raw.trim(); raw.replace(',', '.');
            char *end; float v = strtof(raw.c_str(), &end);
            if (end != raw.c_str() && !*end && isfinite(v)) {
                state.temp_sensors[i] = v; fields |= (1 << i);
            }
        }
        if (!t["hum"].isNull()) {
            String raw_h = t["hum"].as<String>(); raw_h.trim(); raw_h.replace(',', '.');
            char *end_h; float h = strtof(raw_h.c_str(), &end_h);
            if (end_h != raw_h.c_str() && !*end_h && isfinite(h)) {
                state.temp_humidity[i] = h;
            }
        } else {
            state.temp_humidity[i] = 0.0f;
        }
    }

    state.temp_fields = fields;
    int out_idx = constrain(state.outdoor_temp_sensor, 0, TEMP_SOURCE_COUNT - 1);
    state.outdoor_temp = state.temp_sensors[out_idx];
    int in_idx = constrain(state.indoor_temp_sensor, 0, TEMP_SOURCE_COUNT - 1);
    state.indoor_temp = state.temp_sensors[in_idx];
    if (state.fridge_temp_sensor >= 0 && state.fridge_temp_sensor < TEMP_SOURCE_COUNT) {
        state.fridge_temp = state.temp_sensors[state.fridge_temp_sensor];
    } else {
        state.fridge_temp = -999.0f;
    }
    if (state.temp_humidity[in_idx] > 0.5f) {
        state.indoor_humidity = state.temp_humidity[in_idx];
    } else if (out_idx != 2 && in_idx != 2) {
        state.indoor_humidity = state.temp_sensors[2];
    }
}

// GET /heater -> { "autoterm1": { "heatertoggle": bool, "heatstatus": str, "heaterror": str,
//   "targettemp_vanpi": num, "mode": str, "powerlevel": int, "fanspeed": int }, ... }
static void parse_dometic_object(JsonObject rc10);

static void parse_heater_json(const char *payload) {
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, payload)) return;

    bool has_at = doc["autoterm1"].is<JsonObject>();
    JsonObject at;
    if (has_at) at = doc["autoterm1"];
    bool use_at = has_at && (state.heater_type == HEATER_TYPE_AUTOTERM);

    const char *raw_hs = "";
    if (use_at && at.containsKey("heatstatus")) {
        raw_hs = at["heatstatus"].as<const char*>();
    } else if (doc.containsKey("heatstatus")) {
        raw_hs = doc["heatstatus"].as<const char*>();
    }
    if (!raw_hs) raw_hs = "";

    const char *raw_err = "";
    if (use_at && at.containsKey("heaterror")) {
        raw_err = at["heaterror"].as<const char*>();
    } else if (doc.containsKey("heaterror")) {
        raw_err = doc["heaterror"].as<const char*>();
    }
    if (!raw_err) raw_err = "";

    StateLockGuard lock;
    // Read diagnostics before the control hold-off: telemetry keeps updating during interaction.
    JsonObject source = use_at ? at : doc.as<JsonObject>();
    auto &diag = state.heater_diagnostics;
    const bool present = has_at || source.containsKey("heatstatus") || source.containsKey("heatvolt") ||
                         source.containsKey("heatfan") || source.containsKey("heattemp") ||
                         source.containsKey("heatpump") || source.containsKey("heatglow");
    diag.begin(millis(), state.data_is_demo, present);
    // The bundled VanPi flow exposes fuel-pump frequency as historical key heatglow.
    const char *keys[4] = {"heatvolt", "heatfan", source.containsKey("heatpump") ? "heatpump" : "heatglow", "heattemp"};
    for (unsigned i = 0; i < 4; ++i) {
        JsonVariant v = source[keys[i]];
        if (validEnergyNumber(v)) {
            float number = jsonFloat(v);
            diag.set(i, number, i == 3 || number >= 0.0f);
        }
    }
    if (is_webasto_heater(state.heater_type)) {
        if (doc.containsKey("heatvolt") && validEnergyNumber(doc["heatvolt"])) {
            state.webasto_voltage = jsonFloat(doc["heatvolt"]);
        }
        if (doc.containsKey("heattemp") && validEnergyNumber(doc["heattemp"])) {
            state.webasto_temp = jsonFloat(doc["heattemp"]);
        }
    }
    if (state.heater_status != raw_hs) {
        state.heater_status = raw_hs;
        state.heater_status.trim();
    }
    if (state.heater_error != raw_err) {
        state.heater_error = raw_err;
    }

    if (is_truma_heater(state.heater_type) && (int32_t)(millis() - state.truma_hold_until) >= 0 && raw_hs && raw_hs[0] != '\0') {
        String rhs = String(raw_hs);
        rhs.toLowerCase();
        if (rhs.indexOf("eco") >= 0 || rhs.indexOf("40") >= 0) {
            state.truma_water_mode = "eco";
        } else if (rhs.indexOf("hot") >= 0 || rhs.indexOf("60") >= 0 || rhs.indexOf("boost") >= 0) {
            state.truma_water_mode = "hot";
        }
    }

    // User is currently interacting with the heater controls -> don't overwrite
    if ((int32_t)(millis() - state.heater_hold_until) < 0) return;

    // Mode
    const char *m = "";
    if (has_at && at.containsKey("mode")) m = at["mode"].as<const char*>();
    else if (doc.containsKey("mode")) m = doc["mode"].as<const char*>();
    if (m) {
        if (strstr(m, "fan") || strstr(m, "vent") || strstr(m, "Fan") || strstr(m, "Vent")) {
            state.heater_vent_mode = true;
            state.heater_power_mode = false;
        } else if (strstr(m, "power") || strstr(m, "Power")) {
            state.heater_vent_mode = false;
            state.heater_power_mode = true;
        } else if (strstr(m, "temp") || strstr(m, "Temp")) {
            state.heater_vent_mode = false;
            state.heater_power_mode = false;
        }
    }

    // Running state: Check all indicators from VanPi / Autoterm / Webasto
    bool is_on = false;
    if (use_at && at.containsKey("heatertoggle")) {
        if (at["heatertoggle"].is<bool>()) is_on = at["heatertoggle"].as<bool>();
        else {
            const char *ht = at["heatertoggle"].as<const char*>();
            if (ht && (strcasecmp(ht, "true") == 0 || strcmp(ht, "1") == 0)) is_on = true;
        }
    }
    if (!is_on && doc.containsKey("heatertoggle")) {
        if (doc["heatertoggle"].is<bool>()) is_on = doc["heatertoggle"].as<bool>();
        else {
            const char *ht = doc["heatertoggle"].as<const char*>();
            if (ht && (strcasecmp(ht, "true") == 0 || strcmp(ht, "1") == 0)) is_on = true;
        }
    }

    auto is_active_status = [](const char *s) {
        if (!s || s[0] == '\0') return false;
        if (strcasecmp(s, "standby") == 0 || strcasecmp(s, "wait") == 0 ||
            strcasecmp(s, "heater off") == 0 || strcasecmp(s, "unknown status") == 0 ||
            strcmp(s, "0") == 0 || strstr(s, "shutting") || strstr(s, "error") ||
            strstr(s, "flame-out") || strstr(s, "no fuel")) {
            return false;
        }
        return true;
    };

    if (is_active_status(raw_hs)) is_on = true;
    if (!is_truma_heater(state.heater_type)) {
        state.heating_on = is_on;
    }

    if (use_at && at.containsKey("targettemp_vanpi")) {
        float t = jsonFloat(at["targettemp_vanpi"]);
        if (t > 0 && !is_truma_heater(state.heater_type)) state.target_temp = t;
    } else if (doc.containsKey("targettemp_vanpi")) {
        float t = jsonFloat(doc["targettemp_vanpi"]);
        if (t > 0 && !is_truma_heater(state.heater_type)) state.target_temp = t;
    }

    if (state.heater_vent_mode) {
        if (has_at && at.containsKey("fanspeed")) {
            int f = jsonInt(at["fanspeed"]);
            if (f > 0) state.heater_power_level = f;
        }
    } else if (has_at && at.containsKey("powerlevel")) {
        int p = jsonInt(at["powerlevel"]);
        if (p > 0) state.heater_power_level = p;
    }

    // --- Truma CI Telemetry Parsing ---
    if (doc.containsKey("truma_CI")) {
        JsonObject truma = doc["truma_CI"];

        bool alive = false;
        String model = "Truma Combi";
        String err = "none";
        String energy = "fuel";
        int power_lim = 900;
        bool has_230v = false;

        if (truma.containsKey("diagnostics")) {
            JsonObject diag = truma["diagnostics"];
            const char *al = diag["alive"] | "";
            alive = (strcasecmp(al, "online") == 0);

            const char *md = diag["model"] | "";
            if (md && md[0] != '\0') model = md;

            const char *er = diag["error"] | "";
            if (er && er[0] != '\0') err = er;

            const char *en = diag["energy"] | "";
            if (en && en[0] != '\0') energy = en;

            power_lim = diag["power_limit"] | 900;

            if (diag.containsKey("flags")) {
                if (diag["flags"].is<const char*>()) {
                    String fl = diag["flags"].as<const char*>();
                    if (fl.indexOf("230") >= 0) has_230v = true;
                } else if (diag["flags"].is<JsonArray>()) {
                    for (JsonVariant v : diag["flags"].as<JsonArray>()) {
                        String s = v.as<String>();
                        if (s.indexOf("230") >= 0) { has_230v = true; break; }
                    }
                }
            }
        }

        state.truma_online = alive;
        state.truma_model = model;
        state.truma_230v = has_230v;
        state.truma_error = err;
        state.truma_power_limit = power_lim;

        // Map energy & power limit to user energy mode: "fuel", "mix1", "mix2", "el1", "el2"
        if (energy.indexOf("mix") >= 0) {
            state.truma_energy_mode = (power_lim >= 1800) ? "mix2" : "mix1";
        } else if (energy.indexOf("elec") >= 0) {
            state.truma_energy_mode = (power_lim >= 1800) ? "el2" : "el1";
        } else {
            state.truma_energy_mode = "fuel";
        }

        // Only update air & water target if user isn't actively adjusting them
        if ((int32_t)(millis() - state.truma_hold_until) >= 0) {
            if (truma.containsKey("air")) {
                JsonObject air = truma["air"];
                if (air.containsKey("target")) {
                    if (air["target"].is<const char*>() && strcmp(air["target"].as<const char*>(), "off") == 0) {
                        state.truma_heating_on = false;
                    } else {
                        float tg = jsonFloat(air["target"]);
                        if (tg >= 5.0f && tg <= 30.0f) {
                            state.truma_target_temp = tg;
                            state.truma_heating_on = true;
                        } else {
                            state.truma_heating_on = false;
                        }
                    }
                }
                const char *am = air["mode"] | "normal";
                state.truma_air_mode = am;
            }

            if (truma.containsKey("water")) {
                JsonObject water = truma["water"];
                String wt = "";
                if (water.containsKey("target")) {
                    if (water["target"].is<int>()) {
                        int t = water["target"].as<int>();
                        if (t == 40) wt = "eco";
                        else if (t >= 60) wt = "hot";
                        else if (t == 0) wt = "off";
                        else wt = String(t);
                    } else if (water["target"].is<const char*>()) {
                        wt = water["target"].as<const char*>();
                    }
                } else if (water.containsKey("mode")) {
                    wt = water["mode"].as<String>();
                } else if (water.containsKey("status")) {
                    wt = water["status"].as<String>();
                }
                wt.toLowerCase();
                if (wt == "40" || wt.indexOf("eco") >= 0) wt = "eco";
                else if (wt == "60" || wt.indexOf("hot") >= 0 || wt.indexOf("boost") >= 0) wt = "hot";
                else if (wt == "0" || wt == "off" || wt == "false") wt = "off";

                if (wt.length() > 0) state.truma_water_mode = wt;
            }

            // Fallback from raw heatstatus if water target not explicitly provided
            if (state.truma_water_mode == "off" && raw_hs && raw_hs[0] != '\0') {
                String rhs = String(raw_hs);
                rhs.toLowerCase();
                if (rhs.indexOf("eco") >= 0 || rhs.indexOf("40") >= 0) {
                    state.truma_water_mode = "eco";
                } else if (rhs.indexOf("hot") >= 0 || rhs.indexOf("60") >= 0 || rhs.indexOf("boost") >= 0) {
                    state.truma_water_mode = "hot";
                }
            }
        }

        // If Truma is the active heater type, sync main heater state
        if (is_truma_heater(state.heater_type)) {
            state.heating_on = state.truma_heating_on;
            state.target_temp = state.truma_target_temp;
            if (state.truma_heating_on) {
                if (state.truma_water_mode == "eco") state.heater_status = "Heizt | Boiler Eco (40 C)";
                else if (state.truma_water_mode == "hot") state.heater_status = "Heizt | Boiler Hot (60 C)";
                else state.heater_status = "Heizt";
            } else if (state.truma_water_mode != "off" && state.truma_water_mode.length() > 0) {
                if (state.truma_water_mode == "eco") state.heater_status = "Warmwasser Eco (40 C)";
                else if (state.truma_water_mode == "hot") state.heater_status = "Warmwasser Hot (60 C)";
                else state.heater_status = "Warmwasser Aktiv";
            } else {
                state.heater_status = "Standby";
            }
            state.heater_error = (state.truma_error != "none") ? state.truma_error : "no";
        }
    }

    // --- Dometic RC10 CI Telemetry Parsing (if present in /heater) ---
    if (doc.containsKey("dometic_rc10_CI") && doc["dometic_rc10_CI"].is<JsonObject>()) {
        parse_dometic_object(doc["dometic_rc10_CI"].as<JsonObject>());
    }
}

// --- Dometic RC10 CI-Bus Standalone Parsing ---
static void parse_dometic_object(JsonObject rc10) {
    if (rc10.isNull() || rc10.size() == 0) return;

    StateLockGuard lock;
    state.fridge_last_update = millis();

    if (rc10.containsKey("diagnostics") && rc10["diagnostics"].is<JsonObject>()) {
        JsonObject diag = rc10["diagnostics"];
        const char *err = diag["error"] | "none";
        strncpy(state.fridge_error, err, sizeof(state.fridge_error) - 1);
        state.fridge_error[sizeof(state.fridge_error) - 1] = '\0';
    }

    if (rc10.containsKey("status") && rc10["status"].is<JsonObject>()) {
        JsonObject st = rc10["status"];
        if (st.containsKey("power_source")) {
            const char *ps = st["power_source"] | "unk";
            strncpy(state.fridge_power_source, ps, sizeof(state.fridge_power_source) - 1);
            state.fridge_power_source[sizeof(state.fridge_power_source) - 1] = '\0';
        }
        if (st.containsKey("compressor_on")) {
            state.fridge_compressor = st["compressor_on"].as<bool>();
        }
        if (st.containsKey("condensor_fan_on")) {
            state.fridge_fan = st["condensor_fan_on"].as<bool>();
        }

        // Only update control targets if user hold-off has expired
        if ((int32_t)(millis() - state.fridge_hold_until) >= 0) {
            if (st.containsKey("power_on")) {
                state.fridge_power = st["power_on"].as<bool>();
            }
            if (st.containsKey("user_mode")) {
                const char *m = st["user_mode"] | "performance";
                if (strcasecmp(m, "silent") == 0) state.fridge_mode = 1;
                else if (strcasecmp(m, "turbo") == 0) state.fridge_mode = 2;
                else state.fridge_mode = 0;
            }
            if (st.containsKey("set_step")) {
                int step = st["set_step"].as<int>();
                if (step >= 1 && step <= 5) state.fridge_step = (uint8_t)step;
            }
        }
    }
}

static void parse_dometic_json(const char *payload) {
    if (!payload || payload[0] == '\0' || strcmp(payload, "null") == 0) return;
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, payload)) return;
    if (doc.is<JsonObject>()) {
        parse_dometic_object(doc.as<JsonObject>());
    }
}

// GET /position_sensor/?request=true -> { "x_angle": float|str, "y_angle": float|str }
static void parse_position_json(const char *payload) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    if (doc.containsKey("x_angle")) state.roll_angle  = jsonFloat(doc["x_angle"]);
    if (doc.containsKey("y_angle")) state.pitch_angle = jsonFloat(doc["y_angle"]);
}

// GET /info -> {"VanPi_Ctrl":"v2.1.0","install_date":"07/28/2026, 17:31:01","current_date":"14/10/2026, 18:30:35","uptime":"...","ttgo":"7d, 14h"}
static void parse_info_json(const char *payload) {
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, payload)) return;

    StateLockGuard lock;
    if (doc.containsKey("current_date")) {
        const char *cd = doc["current_date"];
        if (cd && strlen(cd) >= 19) {
            state.vanpi_current_date = cd;
            int d = 0, mo = 0, y = 0, h = 0, mi = 0, s = 0;
            // Support Old Core (DD/MM/YYYY, HH:MM:SS) and CorePro (DD.MM.YYYY, HH:MM:SS)
            int matched = sscanf(cd, "%d/%d/%d, %d:%d:%d", &d, &mo, &y, &h, &mi, &s);
            if (matched < 6) matched = sscanf(cd, "%d.%d.%d, %d:%d:%d", &d, &mo, &y, &h, &mi, &s);
            if (matched < 6) matched = sscanf(cd, "%d.%d.%d %d:%d:%d", &d, &mo, &y, &h, &mi, &s);
            if (matched < 6) matched = sscanf(cd, "%d/%d/%d %d:%d:%d", &d, &mo, &y, &h, &mi, &s);
            if (matched < 6 && sscanf(cd, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) matched = 6;
            if (matched == 6) {
                struct tm t = {};
                t.tm_year = y - 1900;
                t.tm_mon  = mo - 1;
                t.tm_mday = d;
                t.tm_hour = h;
                t.tm_min  = mi;
                t.tm_sec  = s;
                t.tm_isdst = -1;
                time_t epoch = mktime(&t);
                if (epoch > 100000) {
                    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
                    settimeofday(&tv, NULL);
                }
            }
        }
    }
    if (doc.containsKey("ttgo")) {
        const char *tt = doc["ttgo"];
        if (tt) {
            String s = tt;
            s.replace("\xE2\x80\x94", "--"); // em-dash U+2014
            s.replace("\xE2\x80\x93", "-");  // en-dash U+2013
            state.time_to_go = s;
        }
    }
}


static unsigned long vanpi_fail_backoff_until = 0;
static int consecutive_main_errors = 0;

static void fetch_endpoint(const char* endpoint, void (*parser)(const char*)) {
    if (WiFi.status() != WL_CONNECTED) {
        state.vanpi_connected = false;
        return;
    }
    if (state.vanpi_ip.length() < 7) {
        state.vanpi_connected = false;
        return;
    }

    // If host is unreachable, don't block loop() repeatedly; wait before retrying
    if (millis() < vanpi_fail_backoff_until) {
        return;
    }

    HTTPClient http;
    http.begin(get_url() + endpoint);
    http.setConnectTimeout(1000);
    http.setTimeout(1500);
    int code = http.GET();
    if (code == 200) {
        state.vanpi_connected = true;
        consecutive_main_errors = 0;
        vanpi_fail_backoff_until = 0;
        String payload = http.getString();
        if (payload.length() > 0 && parser) {
            parser(payload.c_str());
        }
    } else {
        consecutive_main_errors++;
        Serial.printf("[HTTP] GET %s -> %d (consec errs: %d)\n", endpoint, code, consecutive_main_errors);
        if (consecutive_main_errors >= 4) {
            state.vanpi_connected = false;
            vanpi_fail_backoff_until = millis() + 4000;
            Serial.println("[HTTP] 4 consecutive failures, backoff 4s");
        }
    }
    http.end();
}

static void fetch_optional_endpoint(const char* endpoint, void (*parser)(const char*), void (*on_error)()) {
    if (WiFi.status() != WL_CONNECTED || state.vanpi_ip.length() < 7) {
        if (on_error) on_error();
        return;
    }

    HTTPClient http;
    http.begin(get_url() + endpoint);
    http.setConnectTimeout(1000);
    http.setTimeout(1500);
    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        if (payload.length() > 0 && parser) {
            parser(payload.c_str());
        }
    } else {
        // Optional endpoint missing (404) or failed: do NOT trigger vanpi_fail_backoff_until!
        if (on_error) on_error();
    }
    http.end();
}

static unsigned long last_shore_success_ms = 0;
static int shore_404_count = 0;
static int shore_fail_count = 0;

static void fetch_shore_data() {
    if (!state.power_show_shore || shore_404_count >= 3) return;
    if (WiFi.status() != WL_CONNECTED || state.vanpi_ip.length() < 7) return;

    HTTPClient http;
    http.begin(get_url() + "/shore");
    http.setConnectTimeout(1000);
    http.setTimeout(1500);
    int code = http.GET();
    if (code == 200) {
        shore_404_count = 0;
        shore_fail_count = 0;
        last_shore_success_ms = millis();
        String payload = http.getString();
        if (payload.length() > 0) {
            parse_shore_json(payload.c_str());
        }
        Serial.printf("[HTTP-OPT] /shore -> 200 OK: %s\n", payload.c_str());
    } else if (code == 404) {
        shore_404_count++;
        if (shore_404_count >= 2) {
            StateLockGuard lock;
            state.shore_available = false;
            state.shore_connected = false;
            state.shore_power = 0.0f;
            state.shore_voltage = 0.0f;
            state.shore_current = 0.0f;
            if (shore_404_count == 2) {
                Serial.println("[HTTP-OPT] /shore -> 404 Not Found (marked unavailable)");
            }
        }
    } else {
        shore_fail_count++;
        Serial.printf("[HTTP-OPT] /shore -> error %d\n", code);
        // Only mark unavailable if no success for > 30 seconds
        if ((last_shore_success_ms > 0 && millis() - last_shore_success_ms > 30000) ||
            (last_shore_success_ms == 0 && shore_fail_count >= 5)) {
            StateLockGuard lock;
            state.shore_available = false;
            state.shore_connected = false;
            state.shore_power = 0.0f;
            state.shore_voltage = 0.0f;
            state.shore_current = 0.0f;
        }
    }
    http.end();
}

static unsigned long last_booster_success_ms = 0;
static int booster_404_count = 0;
static int booster_fail_count = 0;

static void fetch_booster_data() {
    if (!state.power_show_alt || booster_404_count >= 3) return;
    if (WiFi.status() != WL_CONNECTED || state.vanpi_ip.length() < 7) return;

    HTTPClient http;
    http.begin(get_url() + "/booster");
    http.setConnectTimeout(1000);
    http.setTimeout(1500);
    int code = http.GET();
    if (code == 200) {
        booster_404_count = 0;
        booster_fail_count = 0;
        last_booster_success_ms = millis();
        String payload = http.getString();
        if (payload.length() > 0) {
            parse_booster_json(payload.c_str());
        }
        Serial.printf("[HTTP-OPT] /booster -> 200 OK: %s\n", payload.c_str());
    } else if (code == 404) {
        booster_404_count++;
        if (booster_404_count >= 2) {
            StateLockGuard lock;
            state.alt_available = false;
            state.alt_active = false;
            state.alt_power = 0.0f;
            state.alt_current = 0.0f;
            if (booster_404_count == 2) {
                Serial.println("[HTTP-OPT] /booster -> 404 Not Found (marked unavailable)");
            }
        }
    } else {
        booster_fail_count++;
        Serial.printf("[HTTP-OPT] /booster -> error %d\n", code);
        if ((last_booster_success_ms > 0 && millis() - last_booster_success_ms > 30000) ||
            (last_booster_success_ms == 0 && booster_fail_count >= 5)) {
            StateLockGuard lock;
            state.alt_available = false;
            state.alt_active = false;
            state.alt_power = 0.0f;
            state.alt_current = 0.0f;
        }
    }
    http.end();
}

void http_loop() {
    state.wifi_connected = WiFi.status() == WL_CONNECTED;
    state.wifi_rssi = state.wifi_connected ? WiFi.RSSI() : 0;
    static bool demo_relays_seeded = false;
    static bool demo_dimmers_seeded = false;
    static uint32_t last_uart_log_ms = 0;
    if (state.data_is_demo != state.debug_mode) {
        state.battery_fields = state.solar_fields = state.temp_fields = state.tank_fields = 0;
        state.relay_fields = state.dimmer_fields = 0;
        state.bat_voltage = state.bat_current = state.starter_voltage = 0;
        state.bat_soc = 0;
        state.solar_power = state.solar_current = state.solar_voltage = 0;
        state.shore_power = state.shore_voltage = state.shore_current = 0.0f;
        state.shore_connected = false;
        state.alt_power = state.alt_current = 0.0f;
        state.alt_active = false;
        state.indoor_temp = state.outdoor_temp = 0;
        state.heating_on = state.fan_on = false;
        for (int i = 0; i < TEMP_SOURCE_COUNT; ++i) { state.temp_sensors[i] = 0; state.temp_humidity[i] = 0.0f; state.temp_is_humidity[i] = false; }
        for (int i = 0; i < 4; ++i) state.tank_level[i] = 0;
        for (int i = 0; i < 16; ++i) {
            state.relay_hold_until[i] = 0;
            state.switch_state[i] = false;
        }
        for (int i = 0; i < 10; ++i) {
            state.dimmer_hold_until[i] = 0;
            state.dimmer_val[i] = 0;
        }
        demo_relays_seeded = demo_dimmers_seeded = false;
        shore_404_count = booster_404_count = 0;
        poll_step = 0;
        last_http_poll = 0;
        state.vanpi_connected = false;
        state.heater_diagnostics = HeaterDiagnostics();
        state.data_is_demo = state.debug_mode;
    }

    const bool low_power_poll = !display_is_on || ui_screensaver_is_active();
    // =========================================================================
    // Simulation / Debug Mode: Use realistic dummy JSON payloads with strings
    // =========================================================================
    if (state.debug_mode) {
        uint32_t demo_interval = low_power_poll ? 2500UL : 500UL;
        if (millis() - last_http_poll > demo_interval) {
            switch (poll_step) {
                case 0: {
                    // String-encoded float/int values with all energy producers simulated
                    float phase = (millis() % 240000UL) * (2.0f * PI / 240000.0f);
                    float sim_v_starter = 14.1f + 0.15f * sinf(phase);
                    float sim_p_shore = 280.0f + 60.0f * cosf(phase);
                    float sim_p_alt = 190.0f + 30.0f * sinf(phase * 1.5f);
                    float sim_v_bat = 13.4f + 0.12f * cosf(phase);
                    float sim_i_bat = 18.5f + 4.0f * sinf(phase);
                    int sim_soc = (int)lroundf(78 + 14 * cosf(phase));

                    char data[256];
                    snprintf(data, sizeof(data),
                        "{\"VoltB\":%.2f,\"Ampere\":%.2f,\"battsoc\":%d,\"starter_voltage\":%.2f,\"shore_power\":%.1f,\"shore_voltage\":230.0,\"shore_current\":%.2f,\"shore_connected\":true,\"alt_power\":%.1f,\"alt_current\":%.1f,\"alt_active\":true}",
                        sim_v_bat, sim_i_bat, constrain(sim_soc, 0, 100), sim_v_starter,
                        sim_p_shore, sim_p_shore / 230.0f,
                        sim_p_alt, sim_p_alt / sim_v_bat);
                    parse_batt_json(data);
                    break;
                }
                case 1: {
                    const char *dummy = state.show_relay_ext ?
                        "{\"Relay1\":{\"state\":true,\"name\":\"Licht Bank\"},\"Relay2\":{\"state\":false,\"name\":\"Kuehlschrank\"},\"Relay3\":{\"state\":true,\"name\":\"Wasserpumpe\"},\"Relay4\":{\"state\":false,\"name\":\"Abwasserventil\"},\"Relay5\":{\"state\":false,\"name\":\"Relais 5\"},\"Relay6\":{\"state\":false,\"name\":\"Relais 6\"},\"Relay7\":{\"state\":false,\"name\":\"Relais 7\"},\"Relay8\":{\"state\":false,\"name\":\"Relais 8\"},\"Relay9\":{\"state\":false,\"name\":\"Aussenlicht\"},\"Relay10\":{\"state\":false,\"name\":\"Heckgarage\"},\"Relay11\":{\"state\":false,\"name\":\"Relais 11\"},\"Relay12\":{\"state\":false,\"name\":\"Relais 12\"},\"Relay13\":{\"state\":false,\"name\":\"Relais 13\"},\"Relay14\":{\"state\":false,\"name\":\"Relais 14\"},\"Relay15\":{\"state\":false,\"name\":\"Relais 15\"},\"Relay16\":{\"state\":false,\"name\":\"Relais 16\"}}" :
                        "{\"Relay1\":{\"state\":true,\"name\":\"Licht Bank\"},\"Relay2\":{\"state\":false,\"name\":\"Kuehlschrank\"},\"Relay3\":{\"state\":true,\"name\":\"Wasserpumpe\"},\"Relay4\":{\"state\":false,\"name\":\"Abwasserventil\"},\"Relay5\":{\"state\":false,\"name\":\"Relais 5\"},\"Relay6\":{\"state\":false,\"name\":\"Relais 6\"},\"Relay7\":{\"state\":false,\"name\":\"Relais 7\"},\"Relay8\":{\"state\":false,\"name\":\"Relais 8\"}}";
                    if (!demo_relays_seeded) { parse_relay_json(dummy); demo_relays_seeded = true; }
                    break;
                }
                case 2: {
                    const char *dummy = state.dimmy_pro ?
                        "{\"dimmer8\":{\"state\":75,\"name\":\"WC / Bad\"},\"dimmer9\":{\"state\":40,\"name\":\"Kueche\"},\"dimmer10\":{\"state\":0,\"name\":\"Tisch\"},\"dimmer11\":{\"state\":0,\"name\":\"Bett\"},\"dimmer12\":{\"state\":0,\"name\":\"Werkstatt\"},\"dimmer13\":{\"state\":0,\"name\":\"Regenrinne\"},\"dimmer14\":{\"state\":0,\"name\":\"Deckenlicht\"},\"dimmer15\":{\"state\":0,\"name\":\"Bodenlicht\"},\"rgbw1\":{\"state\":{\"rgb\":{\"r\":255,\"g\":255,\"b\":255},\"white\":0,\"bri\":80},\"name\":\"Markisenbeleuchtung\"},\"rgbw2\":{\"state\":{\"rgb\":{\"r\":255,\"g\":255,\"b\":255},\"white\":0,\"bri\":30},\"name\":\"Fahrerbereich\"}}" :
                        "{\"dimmer1\":{\"state\":\"75\",\"name\":\"Deckenlampe\"},\"dimmer2\":{\"state\":\"40\",\"name\":\"Kueche\"},\"dimmer3\":{\"state\":\"0\",\"name\":\"Leselicht\"},\"dimmer4\":{\"state\":\"0\",\"name\":\"Dimmer 4\"},\"dimmer5\":{\"state\":\"0\",\"name\":\"Dimmer 5\"},\"dimmer6\":{\"state\":\"0\",\"name\":\"Dimmer 6\"},\"dimmer7\":{\"state\":\"0\",\"name\":\"Dimmer 7\"},\"dimmer8\":{\"state\":\"0\",\"name\":\"Dimmer 8\"}}";
                    if (!demo_dimmers_seeded) { parse_dimmer_json(dummy); demo_dimmers_seeded = true; }
                    break;
                }
                case 3: {
                    const char *dummy = "{\"level1\":{\"state\":\"68\",\"name\":\"Frischwasser\"},\"level2\":{\"state\":\"32\",\"name\":\"Grauwasser\"},\"level3\":{\"state\":\"50\",\"name\":\"Tank 3\"},\"level4\":{\"state\":\"0\",\"name\":\"Tank 4\"}}";
                    parse_level_json(dummy);
                    break;
                }
                case 4: {
                    const char *dummy = "{\"temp1\":{\"state\":\"22.4\",\"name\":\"Innen\"},\"temp2\":{\"state\":\"14.6\",\"name\":\"Aussen\"},\"temp3\":{\"state\":\"52.0\",\"name\":\"Feuchte\"},\"temp4\":{\"state\":\"7.8\",\"name\":\"Kuehlbox\"},"
                                        "\"ruuvitag0\":{\"state\":22.7,\"name\":\"Ruuvi Wohnraum\",\"hum\":48.5},\"ruuvitag1\":{\"state\":15.2,\"name\":\"Ruuvi Aussen\",\"hum\":68.2},\"ruuvitag2\":{\"state\":5.3,\"name\":\"Ruuvi Kuehlschrank\"}}";
                    parse_temp_json(dummy);
                    break;
                }
                case 5: {
                    const char *dummy = "{\"autoterm1\":{\"heatertoggle\":true,\"heatstatus\":\"heating\",\"heaterror\":\"no\",\"targettemp_vanpi\":\"21.5\",\"powerlevel\":\"5\",\"fanspeed\":0,\"heatvolt\":13.0,\"heatfan\":\"4200\",\"heatglow\":1.5,\"heattemp\":\"144\",\"mode\":\"temp mode\"}}";
                    parse_heater_json(dummy);
                    break;
                }
                case 6: {
                    float phase = (millis() % 180000UL) * (2.0f * PI / 180000.0f);
                    float volts = 18.2f + .3f * sinf(phase), watts = 195.0f + 65.0f * sinf(phase);
                    char data[144];
                    snprintf(data, sizeof(data), "{\"mppt_pv_watts\":%.2f,\"mppt_pv_amps\":%.2f,\"mppt_pv_volts\":%.2f}", watts, watts / volts, volts);
                    parse_mppt_json(data);
                    break;
                }
                case 7: {
                    const char *dummy = "{\"x_angle\":\"-0.8\",\"y_angle\":\"1.4\"}";
                    parse_position_json(dummy);
                    break;
                }
                case 8: {
                    if (state.show_wrelay) {
                        const char *dummy = "{\"WifiRelay1\":{\"state\":true,\"name\":\"Aussenlicht\"},\"WifiRelay2\":{\"state\":false,\"name\":\"Kofferraum\"},\"WifiRelay3\":{\"state\":false,\"name\":\"Markise\"},\"WifiRelay4\":{\"state\":false,\"name\":\"WRelais 4\"},\"WifiRelay5\":{\"state\":false,\"name\":\"WRelais 5\"},\"WifiRelay6\":{\"state\":false,\"name\":\"WRelais 6\"},\"WifiRelay7\":{\"state\":false,\"name\":\"WRelais 7\"},\"WifiRelay8\":{\"state\":false,\"name\":\"WRelais 8\"}}";
                        parse_wrelay_json(dummy);
                    }
                    break;
                }
                case 9: {
                    time_t rawtime;
                    time(&rawtime);
                    struct tm ti;
                    bool valid = (rawtime > 100000) && localtime_r(&rawtime, &ti);
                    char dummy[256];
                    snprintf(dummy, sizeof(dummy),
                        "{\"VanPi_Ctrl\":\"v2.1.0\",\"current_date\":\"%02d/%02d/%04d, %02d:%02d:%02d\",\"ttgo\":\"7d, 14h\"}",
                        valid ? ti.tm_mday : 14, valid ? (ti.tm_mon + 1) : 10, valid ? (ti.tm_year + 1900) : 2026,
                        valid ? ti.tm_hour : 18, valid ? ti.tm_min : 30, valid ? ti.tm_sec : 35);
                    parse_info_json(dummy);
                    break;
                }
                case 10: {
                    if (state.power_show_shore) {
                        float phase = (millis() % 240000UL) * (2.0f * PI / 240000.0f);
                        float sim_p_shore = 280.0f + 60.0f * cosf(phase);
                        char dummy[128];
                        snprintf(dummy, sizeof(dummy),
                            "{\"connected\":true,\"power\":%.1f,\"voltage\":230.0,\"current\":%.2f}",
                            sim_p_shore, sim_p_shore / 230.0f);
                        parse_shore_json(dummy);
                    } else {
                        state.shore_available = true;
                        state.shore_connected = false;
                        state.shore_power = 0.0f;
                    }
                    break;
                }
                case 11: {
                    if (state.power_show_alt) {
                        float phase = (millis() % 240000UL) * (2.0f * PI / 240000.0f);
                        float sim_p_alt = 190.0f + 30.0f * sinf(phase * 1.5f);
                        char dummy[128];
                        snprintf(dummy, sizeof(dummy),
                            "{\"active\":true,\"power\":%.1f,\"voltage\":14.2,\"current\":%.2f}",
                            sim_p_alt, sim_p_alt / 13.4f);
                        parse_booster_json(dummy);
                    } else {
                        state.alt_available = true;
                        state.alt_active = false;
                        state.alt_power = 0.0f;
                    }
                    break;
                }
                case 12: {
                    if (state.tab_enabled[TAB_FRIDGE]) {
                        state.fridge_last_update = millis();
                        if (state.fridge_power) {
                            state.fridge_compressor = ((millis() / 15000) % 2 == 0);
                            state.fridge_fan = state.fridge_compressor;
                        } else {
                            state.fridge_compressor = false;
                            state.fridge_fan = false;
                        }
                    }
                    break;
                }
            }

            if (poll_step == 7 && !low_power_poll && (millis() - last_uart_log_ms >= 60000UL)) {
                last_uart_log_ms = millis();
                Serial.printf("[CAMPER_UART] Batt: %.2fV, %.2fA, %d%% | Solar: %.1fW | TempIn: %.1fC, Out: %.1fC | Level: Pitch=%.1f°, Roll=%.1f°\n",
                              state.bat_voltage, state.bat_current, state.bat_soc,
                              state.solar_power,
                              state.indoor_temp, state.outdoor_temp,
                              state.pitch_angle, state.roll_angle);
            }

            poll_step++;
            if (poll_step > 12) poll_step = 0;
            last_http_poll = millis();
        }
        return;
    }

    // =========================================================================
    // Normal Live Mode: Poll VanPi via HTTP
    // =========================================================================
    if (WiFi.status() != WL_CONNECTED) {
        state.wifi_connected = false;
        state.vanpi_connected = false;
        return;
    }
    state.wifi_connected = true;
    state.wifi_rssi = WiFi.RSSI();

    if (millis() < vanpi_fail_backoff_until) {
        state.vanpi_connected = false;
        return;
    }

    static uint32_t last_level_fast_poll = 0;
    if (!low_power_poll && ui_is_level_tab_active() && millis() - last_level_fast_poll >= 400) {
        last_level_fast_poll = millis();
        fetch_endpoint("/position_sensor/?request=true", parse_position_json);
    }

    uint32_t poll_interval_ms = low_power_poll ? 2500UL : (ui_is_level_tab_active() ? 1000UL : 500UL);
    if (millis() - last_http_poll > poll_interval_ms) {
        // When display is completely off, skip UI-only endpoints (/relay, /dimmer, /level, /position_sensor, /wrelay)
        // and only poll endpoints needed for background history, heater status, or clock sync.
        if (!display_is_on) {
            while (poll_step == 1 || poll_step == 2 || poll_step == 3 || poll_step == 7 || poll_step == 8) {
                poll_step++;
            }
        }

        switch (poll_step) {
            case 0: fetch_endpoint("/batt",    parse_batt_json);    break;
            case 1: fetch_endpoint("/relay",   parse_relay_json);   break;
            case 2: fetch_endpoint("/dimmer",  parse_dimmer_json);  break;
            case 3: fetch_endpoint("/level",   parse_level_json);   break;
            case 4: fetch_endpoint("/temp",    parse_temp_json);    break;
            case 5:
                if (state.tab_enabled[TAB_CLIMATE] || state.tab_enabled[TAB_TRUMA] || state.tab_enabled[TAB_FRIDGE] || state.heater_type != HEATER_TYPE_NONE) {
                    fetch_endpoint("/heater", parse_heater_json);
                }
                break;
            case 6: fetch_endpoint("/mppt/",   parse_mppt_json);    break;
            case 7:
                if (state.tab_enabled[TAB_LEVEL] && !ui_is_level_tab_active()) {
                    fetch_endpoint("/position_sensor/?request=true", parse_position_json);
                }
                break;
            case 8:
                if (state.show_wrelay) {
                    fetch_optional_endpoint("/wrelay", parse_wrelay_json, nullptr);
                }
                break;
            case 9:
                fetch_endpoint("/info", parse_info_json);
                break;
            case 10:
                fetch_shore_data();
                break;
            case 11:
                fetch_booster_data();
                break;
            case 12:
                if (state.tab_enabled[TAB_FRIDGE]) {
                    fetch_endpoint("/dometic_rc10_ci/", parse_dometic_json);
                }
                break;
        }

        if (poll_step == 7 && !low_power_poll && (millis() - last_uart_log_ms >= 60000UL)) {
            last_uart_log_ms = millis();
            Serial.printf("[CAMPER_UART] Batt: %.2fV, %.2fA, %d%% | Solar: %.1fW | TempIn: %.1fC | Level: Pitch=%.1f°, Roll=%.1f°\n",
                          state.bat_voltage, state.bat_current, state.bat_soc,
                          state.solar_power,
                          state.indoor_temp,
                          state.pitch_angle, state.roll_angle);
            Serial.printf("[CAMPER_UART] Heater: on=%d status='%s' err='%s' mode=%s target=%.1f lvl=%d | Dim: %d %d %d %d\n",
                          state.heating_on, state.heater_status.c_str(), state.heater_error.c_str(),
                          state.heater_vent_mode ? "vent" : (state.heater_power_mode ? "power" : "temp"),
                          state.target_temp, state.heater_power_level,
                          state.dimmer_val[0], state.dimmer_val[1], state.dimmer_val[2], state.dimmer_val[3]);
            Serial.printf("[CAMPER_UART] WRelay: [0]='%s' [1]='%s' fields=0x%02X\n",
                          state.wrelay_names[0].c_str(), state.wrelay_names[1].c_str(), state.wrelay_fields);
            Serial.printf("[CAMPER_UART] Power: Shore=%.1fW (avail=%d, conn=%d) | Booster=%.1fW (avail=%d, act=%d)\n",
                          state.shore_power, state.shore_available, state.shore_connected,
                          state.alt_power, state.alt_available, state.alt_active);
        }

        poll_step++;
        if (poll_step > 12) poll_step = 0;
        last_http_poll = millis();
    }
}

// --- Publishing (Non-blocking: pushes to FreeRTOS queue, processed on Core 0) ---

static void http_put(String endpoint) {
    if (state.debug_mode) return;
    if (WiFi.status() != WL_CONNECTED) return;
    if (state.vanpi_ip.length() < 7) return;

    HTTPClient http;
    http.begin(get_url() + endpoint);
    http.setConnectTimeout(400);
    http.setTimeout(1500);
    http.addHeader("Content-Type", "application/json");
    int code = http.PUT("{}");
    if (code != 200) {
        Serial.printf("[HTTP] PUT %s -> %d\n", endpoint.c_str(), code);
    }
    http.end();
}

static RelayClickFilter relay_click_filter;

bool http_publish_switch(int index, bool on) {
    int max_r = state.show_relay_ext ? 16 : 8;
    if (index < 0 || index >= max_r || !relay_click_filter.accept(index, millis())) return false;
    state.switch_state[index] = on;
    state.relay_hold_until[index] = millis() + 2000;
    char path[64];
    snprintf(path, sizeof(path), "/relay/%d/%s", index + 1, on ? "true" : "false");
    queue_cmd(path);
    return true;
}

bool http_publish_wrelay(int index, bool on) {
    if (index < 0 || index >= 8 || !relay_click_filter.accept(index + 16, millis())) return false;
    state.wrelay_state[index] = on;
    state.wrelay_hold_until[index] = millis() + 2000;
    char path[64];
    snprintf(path, sizeof(path), "/wrelay/%d/%s", index + 1, on ? "true" : "false");
    queue_cmd(path);
    return true;
}

void http_publish_dimmer(int index, int val) {
    int max_dim = state.dimmy_pro ? 10 : 8;
    if (index < 0 || index >= max_dim) return;
    val = constrain(val, 0, 100);
    state.dimmer_val[index] = val;
    int ch = state.dimmy_pro ? (index + 8) : (index + 1);
    char path[64];
    snprintf(path, sizeof(path), "/dimmer/%d/%d", ch, val);
    queue_cmd(path);
}

void http_publish_heater_cmd(String mode, int value) {
    char path[64];
    snprintf(path, sizeof(path), "/autoterm/%s/%d", mode.c_str(), value);
    queue_cmd(path);
}

void http_publish_webasto(bool on, int temp) {
    state.heater_hold_until = millis() + 5000;
    char path[64];
    if (!on) {
        state.heating_on = false;
        snprintf(path, sizeof(path), "/heater/false");
    } else {
        state.heating_on = true;
        if (temp > 0) {
            state.target_temp = (float)temp;
            snprintf(path, sizeof(path), "/heater/true/%d", constrain(temp, 5, 35));
        } else {
            snprintf(path, sizeof(path), "/heater/true");
        }
    }
    queue_cmd(path);
}

void http_publish_truma_air(int temp) {
    state.truma_hold_until = millis() + 4000;
    char path[64];
    if (temp <= 0) {
        state.truma_heating_on = false;
        snprintf(path, sizeof(path), "/truma_ci/air/off");
    } else {
        state.truma_heating_on = true;
        state.truma_target_temp = (float)temp;
        snprintf(path, sizeof(path), "/truma_ci/air/%d", temp);
    }
    if (is_truma_heater(state.heater_type)) {
        state.heating_on = state.truma_heating_on;
        state.target_temp = state.truma_target_temp;
    }
    queue_cmd(path);
}

void http_publish_truma_water(const char *mode) {
    state.truma_hold_until = millis() + 4000;
    state.truma_water_mode = mode ? mode : "off";
    char path[64];
    snprintf(path, sizeof(path), "/truma_ci/water/%s", state.truma_water_mode.c_str());
    queue_cmd(path);
}

void http_publish_truma_energy(const char *mode) {
    state.truma_hold_until = millis() + 4000;
    state.truma_energy_mode = mode ? mode : "fuel";
    char path[64];
    snprintf(path, sizeof(path), "/truma_ci/energymix/%s", state.truma_energy_mode.c_str());
    queue_cmd(path);
}

void http_publish_fridge(bool power, const char *mode, int step) {
    state.fridge_hold_until = millis() + 4000;
    state.fridge_power = power;
    if (mode) {
        if (strcasecmp(mode, "silent") == 0) state.fridge_mode = 1;
        else if (strcasecmp(mode, "turbo") == 0) state.fridge_mode = 2;
        else state.fridge_mode = 0;
    }
    state.fridge_step = constrain(step, 1, 5);

    const char *mode_str = (state.fridge_mode == 1) ? "silent" : ((state.fridge_mode == 2) ? "turbo" : "performance");
    char path[64];
    snprintf(path, sizeof(path), "/dometic_rc10_ci/%s/%s/%d", power ? "on" : "off", mode_str, state.fridge_step);
    queue_cmd(path);
}

void http_calibrate_position() {
    Serial.println("[HTTP] Calibrate position sensor requested");
    queue_cmd("/position_sensor/?request=calibrate");
}

// Dedicated FreeRTOS background task running on Core 0
static void http_background_task(void *pvParameters) {
    for (;;) {
        // 1. Process any pending outgoing commands with immediate priority
        HttpCmd cmd;
        while (http_cmd_queue && xQueueReceive(http_cmd_queue, &cmd, 0) == pdTRUE) {
            http_put(String(cmd.path));
        }

        maxxfan_worker();

        // 2. Poll endpoints
        http_loop();

        // Short sleep so other Core 0 tasks (WiFi driver, idle) get time
        uint32_t worker_sleep_ms = (!display_is_on || ui_screensaver_is_active()) ? 50 : 15;
        vTaskDelay(pdMS_TO_TICKS(worker_sleep_ms));
    }
}

bool http_start_task() {
    static TaskHandle_t worker = NULL;
    if (worker) return true;
    http_init();
    if (!http_cmd_queue) {
        Serial.println("[HTTP] ERROR: command queue allocation failed; data worker not started");
        return false;
    }
    // Pin to Core 0 (Core 1 is 100% dedicated to UI / LVGL!)
    BaseType_t result = xTaskCreatePinnedToCore(
        http_background_task,
        "http_task",
        8192,        // 8 KB stack (ensures ample headroom for HTTPClient, TCP & JSON parsing)
        NULL,
        1,           // Priority 1
        &worker,
        0            // Core 0
    );
    if (result != pdPASS) {
        worker = NULL;
        Serial.println("[HTTP] ERROR: worker allocation failed; Demo/Live data unavailable");
        return false;
    }
    Serial.println("[HTTP] Background worker pinned to Core 0");
    return true;
}
