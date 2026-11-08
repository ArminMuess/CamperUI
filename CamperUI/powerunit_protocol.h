#pragma once
// Pekaway Power Unit over MQTT (bridge pu_ble_mqtt.py in VanPi Core OS 2.1.1+).
//   <prefix>/<device id>/state   status JSON (schema 1), retained, about once per second
//   <prefix>/<device id>/health  {"connected": true|false, ...}, retained (last will = offline)
//   <prefix>/<device id>/cmd     plain-text command, never retained
// Pure helpers without Arduino dependencies so they can be tested on the host.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ArduinoJson.h>

namespace powerunit {

constexpr const char *TOPIC_PREFIX = "pekaway/power_unit";
constexpr uint32_t STALE_MS = 10000;          // no state for this long = data outdated
constexpr uint32_t COMMAND_TIMEOUT_MS = 12000; // no confirmation within this time = failed

enum class Command : uint8_t {
    InverterOn, InverterOff, ReverseOn, ReverseOff,
    ShoreHigh, ShoreLow, AlternatorHigh, AlternatorLow, BuzzerOn, BuzzerOff, Count
};
inline const char *command_text(Command c) {
    switch (c) {
        case Command::InverterOn: return "ac_output_on";
        case Command::InverterOff: return "ac_output_off";
        case Command::ReverseOn: return "reverse_charge_on";
        case Command::ReverseOff: return "reverse_charge_off";
        case Command::ShoreHigh: return "mains_charge_high_on";
        case Command::ShoreLow: return "mains_charge_high_off";
        case Command::AlternatorHigh: return "car_charge_high";
        case Command::AlternatorLow: return "car_charge_low";
        case Command::BuzzerOn: return "buzzer_on";
        case Command::BuzzerOff: return "buzzer_off";
        default: return "";
    }
}
// Switch groups: a command and its opposite share one slot for pending feedback.
enum class Switch : uint8_t { Inverter, Reverse, Shore, Alternator, Buzzer, Count };
inline Switch switch_of(Command c) { return Switch(uint8_t(c) / 2); }
inline bool target_of(Command c) { return (uint8_t(c) % 2) == 0; }
inline Command command_for(Switch s, bool on) { return Command(uint8_t(s) * 2 + (on ? 0 : 1)); }

struct State {
    bool valid = false;
    float soc = NAN, battery_voltage = NAN, battery_current = NAN, battery_power = NAN, battery_temperature = NAN;
    bool battery_charging = false;
    int remaining_minutes = 0;          // 0 = unknown
    char remaining_mode[8] = "";        // "use", "charge", "unknown"
    float starter_voltage = NAN;
    float pv_power = NAN, shore_power = NAN, alternator_power = NAN;
    bool pv_charging = false, shore_charging = false, alternator_charging = false;
    char alternator_mode[10] = "";      // forward, reverse, inactive, unknown
    float ac_power = NAN, ac_voltage = NAN;
    bool switches[uint8_t(Switch::Count)] = {};   // current (or pending target) state per switch
    uint8_t alert_count = 0;
    char alert[64] = "";                // first alert text
    uint8_t pending_count = 0;
};

// German text for the bridge's alert strings; battery alerts keep their detail.
inline void alert_text(const char *in, char *out, size_t len) {
    static const char *const table[][2] = {
        {"Inverter overtemperature", "Wechselrichter überhitzt"},
        {"Inverter overload", "Wechselrichter überlastet"},
        {"Inverter short circuit", "Kurzschluss am Wechselrichter"},
        {"AC charge overtemperature", "Landstrom-Lader überhitzt"},
        {"Charge path overtemperature", "Ladepfad überhitzt"},
        {"Battery high-voltage protection", "Akku-Überspannungsschutz"},
        {"Battery low-voltage warning", "Akku-Unterspannung (Warnung)"},
        {"Battery low-voltage protection", "Akku-Unterspannungsschutz aktiv"},
        {"PV input overvoltage", "Solar-Eingang Überspannung"},
        {"PV input undervoltage", "Solar-Eingang Unterspannung"},
        {"PV output overvoltage", "Solar-Ausgang Überspannung"},
        {"AC startup voltage too low", "AC-Startspannung zu niedrig"},
        {"PV overtemperature", "Solarregler überhitzt"},
    };
    for (auto &t : table) if (!strcmp(in, t[0])) { snprintf(out, len, "%s", t[1]); return; }
    if (!strncmp(in, "Battery ", 8)) { snprintf(out, len, "Akku %s", in + 8); return; }
    snprintf(out, len, "%s", in);
}
inline float number(JsonVariantConst v) {
    return (v.is<float>() || v.is<int>()) ? v.as<float>() : NAN;
}
// Parses one state message. Returns false (and leaves `out` invalid) for anything unexpected.
inline bool parse_state(const char *payload, size_t length, State &out) {
    out = State();
    JsonDocument filter;
    for (const char *k : {"voltage", "current", "power", "temperature", "charging", "soc"}) filter["battery"][k] = true;
    filter["schema"] = true;
    filter["starter_battery"]["voltage"] = true;
    filter["pv"]["power"] = true; filter["pv"]["charging"] = true;
    filter["shore"]["power"] = true; filter["shore"]["charging"] = true; filter["shore"]["high_power"] = true;
    for (const char *k : {"power", "high_power", "charging", "reverse_charging", "mode"}) filter["alternator"][k] = true;
    for (const char *k : {"enabled", "power", "voltage"}) filter["ac_output"][k] = true;
    filter["remaining_time"]["minutes"] = true; filter["remaining_time"]["mode"] = true;
    filter["device"]["buzzer_enabled"] = true;
    filter["alerts"] = true; filter["pending"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter))) return false;
    if ((doc["schema"] | 0) != 1 || !doc["battery"].is<JsonObjectConst>() || !doc["ac_output"]["enabled"].is<bool>())
        return false;
    auto b = doc["battery"];
    out.soc = number(b["soc"]); out.battery_voltage = number(b["voltage"]);
    out.battery_current = number(b["current"]); out.battery_power = number(b["power"]);
    out.battery_temperature = number(b["temperature"]); out.battery_charging = b["charging"] | false;
    out.starter_voltage = number(doc["starter_battery"]["voltage"]);
    out.pv_power = number(doc["pv"]["power"]); out.pv_charging = doc["pv"]["charging"] | false;
    out.shore_power = number(doc["shore"]["power"]); out.shore_charging = doc["shore"]["charging"] | false;
    auto a = doc["alternator"];
    out.alternator_power = number(a["power"]); out.alternator_charging = a["charging"] | false;
    snprintf(out.alternator_mode, sizeof(out.alternator_mode), "%s", a["mode"] | "");
    out.ac_power = number(doc["ac_output"]["power"]); out.ac_voltage = number(doc["ac_output"]["voltage"]);
    out.remaining_minutes = doc["remaining_time"]["minutes"] | 0;
    snprintf(out.remaining_mode, sizeof(out.remaining_mode), "%s", doc["remaining_time"]["mode"] | "");
    out.switches[uint8_t(Switch::Inverter)] = doc["ac_output"]["enabled"];
    out.switches[uint8_t(Switch::Reverse)] = a["reverse_charging"] | false;
    out.switches[uint8_t(Switch::Shore)] = doc["shore"]["high_power"] | false;
    out.switches[uint8_t(Switch::Alternator)] = a["high_power"] | false;
    out.switches[uint8_t(Switch::Buzzer)] = doc["device"]["buzzer_enabled"] | false;
    JsonArrayConst alerts = doc["alerts"];
    out.alert_count = alerts.size() > 255 ? 255 : uint8_t(alerts.size());
    if (out.alert_count) alert_text(alerts[0] | "", out.alert, sizeof(out.alert));
    out.pending_count = uint8_t(doc["pending"].as<JsonArrayConst>().size());
    out.valid = isfinite(out.soc) && out.soc >= 0 && out.soc <= 100;
    return out.valid;
}
inline bool parse_health(const char *payload, size_t length, bool &connected) {
    JsonDocument filter; filter["connected"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter)) || !doc["connected"].is<bool>())
        return false;
    connected = doc["connected"];
    return true;
}
// Command result: sent -> confirmed | timeout | superseded, or error.
enum class Result : uint8_t { Unknown, Sent, Confirmed, Failed };
inline Result parse_result(const char *payload, size_t length, Command &command, char *error, size_t error_len) {
    JsonDocument filter; filter["command"] = true; filter["status"] = true; filter["error"] = true;
    JsonDocument doc;
    if (error_len) error[0] = 0;
    if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter))) return Result::Unknown;
    const char *name = doc["command"] | "", *status = doc["status"] | "";
    bool found = false;
    for (uint8_t i = 0; i < uint8_t(Command::Count); ++i)
        if (!strcmp(name, command_text(Command(i)))) { command = Command(i); found = true; break; }
    if (!found) return Result::Unknown;
    if (!strcmp(status, "sent")) return Result::Sent;
    if (!strcmp(status, "confirmed")) return Result::Confirmed;
    if (error_len) snprintf(error, error_len, "%s", doc["error"] | status);
    return Result::Failed;
}
// Device IDs are the BLE address in hex (e.g. 4142749e4256); allow a safe subset only.
inline bool valid_device_id(const char *id) {
    size_t n = strlen(id);
    if (!n || n > 32) return false;
    for (size_t i = 0; i < n; ++i) {
        char c = id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-')) return false;
    }
    return true;
}
// Splits "<prefix>/<id>/<leaf>"; returns false for foreign topics.
inline bool split_topic(const char *topic, char *id, size_t id_len, char *leaf, size_t leaf_len) {
    size_t p = strlen(TOPIC_PREFIX);
    if (strncmp(topic, TOPIC_PREFIX, p) || topic[p] != '/') return false;
    const char *start = topic + p + 1, *slash = strchr(start, '/');
    if (!slash || size_t(slash - start) >= id_len || strchr(slash + 1, '/')) return false;
    memcpy(id, start, slash - start); id[slash - start] = 0;
    snprintf(leaf, leaf_len, "%s", slash + 1);
    return valid_device_id(id);
}
inline void topic(char *out, size_t len, const char *id, const char *leaf) {
    snprintf(out, len, "%s/%s/%s", TOPIC_PREFIX, *id ? id : "+", leaf);
}
// True if `at` (0 = never) lies less than `limit` ms before `now`. Signed on purpose:
// a timestamp taken in the same or the next millisecond as `now` counts as current.
inline bool within(uint32_t now, uint32_t at, uint32_t limit) {
    return at && int32_t(now - at) < int32_t(limit);
}
// Watt with sensible rounding for the tiles: "--", "850 W", "1,2 kW".
inline void format_power(char *out, size_t len, float w) {
    if (!isfinite(w)) { snprintf(out, len, "--"); return; }
    float a = fabsf(w);
    if (a >= 999.5f) {
        int tenths = int(lroundf(w / 100.f));
        snprintf(out, len, "%s%d,%d kW", tenths < 0 ? "-" : "", abs(tenths) / 10, abs(tenths) % 10);
    } else snprintf(out, len, "%d W", int(lroundf(w)));
}
inline void format_duration(char *out, size_t len, int minutes) {
    if (minutes <= 0) { snprintf(out, len, "--"); return; }
    if (minutes >= 48 * 60) snprintf(out, len, "%d Tage", minutes / 1440);
    else if (minutes >= 60) snprintf(out, len, "%d h %02d min", minutes / 60, minutes % 60);
    else snprintf(out, len, "%d min", minutes);
}
}  // namespace powerunit
