#include "powerunit_client.h"
#include "powerunit_config.h"
#include "system_state.h"
#include "PubSubClient.h"
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_heap_caps.h>
#include "debug_log.h"

using namespace powerunit;
namespace {
portMUX_TYPE guard = portMUX_INITIALIZER_UNLOCKED;
PowerUnitSnapshot model;           // guarded by `guard`
bool wanted = false;               // guarded by `guard`
uint32_t bridge_health_at = 0;     // guarded by `guard`
TaskHandle_t worker = nullptr;
QueueHandle_t commands = nullptr;
struct Request { Command command; };

// No formatting inside critical sections: prepare the text first, then copy it in.
void set_message(const char *text) {
    char buffer[sizeof(model.message)];
    strncpy(buffer, text, sizeof(buffer) - 1); buffer[sizeof(buffer) - 1] = 0;
    portENTER_CRITICAL(&guard); memcpy(model.message, buffer, sizeof(buffer)); portEXIT_CRITICAL(&guard);
}
void command_message(const char *text) {
    char buffer[sizeof(model.command_message)];
    strncpy(buffer, text, sizeof(buffer) - 1); buffer[sizeof(buffer) - 1] = 0;
    uint32_t at = millis() | 1;
    portENTER_CRITICAL(&guard);
    memcpy(model.command_message, buffer, sizeof(buffer)); model.command_message_at = at;
    portEXIT_CRITICAL(&guard);
}
void clear_pending(const char *why) {
    bool any = false;
    portENTER_CRITICAL(&guard);
    for (auto &p : model.pending) { any |= p.active; p.active = false; }
    portEXIT_CRITICAL(&guard);
    if (any && why) command_message(why);
    if (commands) xQueueReset(commands);
}
// MQTT callback (worker task, inside client.loop()).
void received(char *topic, uint8_t *payload, unsigned int length) {
    char id[33], leaf[16];
    if (!split_topic(topic, id, sizeof(id), leaf, sizeof(leaf))) return;
    char current[33];
    portENTER_CRITICAL(&guard); memcpy(current, model.device_id, sizeof(current)); portEXIT_CRITICAL(&guard);
    if (*current && strcmp(current, id)) return;    // another Power Unit on the same broker
    const char *text = reinterpret_cast<const char *>(payload);
    if (!strcmp(leaf, "state")) {
        State st;
        if (!parse_state(text, length, st)) return;
        uint32_t at = millis() | 1;
        portENTER_CRITICAL(&guard);
        if (!*model.device_id) memcpy(model.device_id, id, sizeof(model.device_id));
        model.state = st; model.state_at = at;
        portEXIT_CRITICAL(&guard);
    } else if (!strcmp(leaf, "health")) {
        bool online = false;
        if (!parse_health(text, length, online)) return;
        uint32_t at = millis() | 1;
        portENTER_CRITICAL(&guard);
        if (!*model.device_id) memcpy(model.device_id, id, sizeof(model.device_id));
        model.bridge_offline = !online; bridge_health_at = at;
        portEXIT_CRITICAL(&guard);
    } else if (!strcmp(leaf, "result")) {
        Command c; char error[48];
        Result r = parse_result(text, length, c, error, sizeof(error));
        if (r != Result::Confirmed && r != Result::Failed) return;
        auto &p = model.pending[uint8_t(switch_of(c))];
        bool matches;
        portENTER_CRITICAL(&guard);
        matches = p.active && p.target == target_of(c);
        if (matches) p.active = false;
        portEXIT_CRITICAL(&guard);
        if (!matches) return;
        if (r == Result::Failed) {
            char line[96];
            snprintf(line, sizeof(line), "Power Unit hat den Befehl nicht ausgeführt (%s)",
                     !strcmp(error, "timeout") ? "keine Rückmeldung" : !strcmp(error, "BLE not connected") ? "Bluetooth getrennt" : error);
            command_message(line);
        } else command_message("");
    }
}
void subscribe_all(PubSubClient &client, const char *id) {
    char t[96];
    for (const char *leaf : {"state", "health", "result"}) { topic(t, sizeof(t), id, leaf); client.subscribe(t, 0); }
}
String broker_host(const PowerUnitConfig &c) {
    if (*c.host) return String(c.host);
    StateLockGuard lock;
    String vanpi = state.vanpi_ip;
    int colon = vanpi.indexOf(':');
    return colon > 0 ? vanpi.substring(0, colon) : vanpi;
}
void background(void *) {
    WiFiClient connection;
    PubSubClient client(connection);
    while (!client.setBufferSize(4096)) { set_message("MQTT-Puffer nicht verfügbar"); vTaskDelay(pdMS_TO_TICKS(5000)); }
    client.setSocketTimeout(1); client.setKeepAlive(30); client.setCallback(received);
    uint32_t seen_generation = 0, retry_at = 0;
    String host;
    for (;;) {
        auto endpoint = powerunit_endpoint();
        bool want, ota;
        portENTER_CRITICAL(&guard); want = wanted; portEXIT_CRITICAL(&guard);
        ota = state.web_ota_active;   // plain bool flag, read without the state mutex
        if (endpoint.generation != seen_generation) {
            seen_generation = endpoint.generation;
            if (client.connected()) client.disconnect();
            char configured[33]; memcpy(configured, endpoint.config.device_id, sizeof(configured));
            portENTER_CRITICAL(&guard);
            memcpy(model.device_id, configured, sizeof(model.device_id));
            model.state_at = 0; model.state = State();
            portEXIT_CRITICAL(&guard);
            retry_at = 0;
        }
        if (!want || ota) {
            if (client.connected()) client.disconnect();
            portENTER_CRITICAL(&guard); model.connected = false; portEXIT_CRITICAL(&guard);
            clear_pending(nullptr);
            set_message(ota ? "Pausiert: Firmware-Update" : "Pausiert (Tab nicht sichtbar)");
            vTaskDelay(pdMS_TO_TICKS(250)); continue;
        }
        // Resolve the broker address only when (re)connecting: no String churn in the loop.
        if (!client.connected()) host = broker_host(endpoint.config);
        if (WiFi.status() != WL_CONNECTED || !host.length()) {
            if (client.connected()) client.disconnect();
            portENTER_CRITICAL(&guard); model.connected = false; portEXIT_CRITICAL(&guard);
            clear_pending("Befehl abgebrochen: keine Verbindung");
            set_message(host.length() ? "WLAN getrennt" : "Broker-Adresse fehlt: VanPi-IP oder Zahnrad");
            vTaskDelay(pdMS_TO_TICKS(250)); continue;
        }
        if (!client.connected()) {
            portENTER_CRITICAL(&guard); model.connected = false; portEXIT_CRITICAL(&guard);
            if ((int32_t)(millis() - retry_at) < 0) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
            set_message("Verbinde mit dem MQTT-Broker ...");
            IPAddress ip;
            bool resolved = ip.fromString(host.c_str()) || WiFi.hostByName(host.c_str(), ip) == 1;
            bool ok = false;
            if (resolved && connection.connect(ip, endpoint.config.port, 400)) {
                client.setServer(ip, endpoint.config.port);
                char id[40]; snprintf(id, sizeof(id), "camperui-pu-%llx", (unsigned long long)ESP.getEfuseMac());
                ok = client.connect(id);
            }
            if (!ok) {
                connection.stop(); retry_at = millis() + 10000;
                set_message(client.state() == 4 || client.state() == 5 ? "Broker lehnt die Anmeldung ab"
                                                                       : "Broker nicht erreichbar: Adresse und Port 1883 prüfen");
                continue;
            }
            char id[33];
            portENTER_CRITICAL(&guard); memcpy(id, model.device_id, sizeof(id)); model.connected = true; portEXIT_CRITICAL(&guard);
            subscribe_all(client, id);
            set_message("Verbunden: warte auf Daten der Power Unit");
        }
        client.loop();
        // Send queued commands (only once the device ID is known).
        Request r;
        while (commands && xQueueReceive(commands, &r, 0) == pdTRUE) {
            char id[33], t[96];
            portENTER_CRITICAL(&guard); memcpy(id, model.device_id, sizeof(id)); portEXIT_CRITICAL(&guard);
            topic(t, sizeof(t), id, "cmd");
            if (!*id || !client.publish(t, command_text(r.command), false)) {
                portENTER_CRITICAL(&guard); model.pending[uint8_t(switch_of(r.command))].active = false; portEXIT_CRITICAL(&guard);
                command_message("Befehl konnte nicht gesendet werden");
            }
        }
        uint32_t now = millis();
        bool expired = false;
        portENTER_CRITICAL(&guard);
        model.connected = client.connected();
        // A once-subscribed wildcard is narrowed to the found device on the next reconnect.
        model.fresh = model.connected && within(now, model.state_at, STALE_MS) &&
                      !(model.bridge_offline && bridge_health_at);
        for (auto &p : model.pending)
            if (p.active && int32_t(now - p.since) > int32_t(COMMAND_TIMEOUT_MS)) { p.active = false; expired = true; }
        portEXIT_CRITICAL(&guard);
        if (expired) command_message("Keine Bestätigung von der Power Unit: Zustand prüfen");
        {   // Log every change of connection / data state with the reason (for diagnosis).
            static bool was_connected = false, was_fresh = false;
            portENTER_CRITICAL(&guard);
            bool c = model.connected, f = model.fresh, off = model.bridge_offline; uint32_t at = model.state_at;
            portEXIT_CRITICAL(&guard);
            if (c != was_connected || f != was_fresh) {
                Serial.printf("[POWERUNIT] mqtt %d->%d daten %d->%d (letzte Daten vor %ld ms, bridge_offline=%d, mqtt_state=%d)\n",
                              was_connected, c, was_fresh, f, at ? (long)int32_t(now - at) : 0L, off, client.state());
                was_connected = c; was_fresh = f;
            }
        }
        if (!client.connected()) { clear_pending("Verbindung verloren: Befehl abgebrochen"); set_message("MQTT-Verbindung verloren"); }
        else {
            portENTER_CRITICAL(&guard); bool fresh = model.fresh, offline = model.bridge_offline;
            uint32_t at = model.state_at; portEXIT_CRITICAL(&guard);
            set_message(fresh ? "Verbunden" : offline ? "Pekaway meldet: Power Unit per Bluetooth nicht verbunden"
                              : at ? "Keine aktuellen Daten von der Power Unit"
                                   : "Verbunden: warte auf Daten der Power Unit");
        }
        static uint32_t last_log = 0;
        if (now - last_log >= 60000) {
            last_log = now;
            portENTER_CRITICAL(&guard); bool fresh = model.fresh; float soc = model.state.soc;
            char id[33]; memcpy(id, model.device_id, sizeof(id)); portEXIT_CRITICAL(&guard);
            Serial.printf("[POWERUNIT] mqtt=%d daten=%d id=%s soc=%.0f stack_frei=%u heap=%u\n", client.connected(), fresh,
                          *id ? id : "-", soc, unsigned(uxTaskGetStackHighWaterMark(nullptr)),
                          unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
}  // namespace

void powerunit_init() {
    powerunit_config_load();
    if (worker) return;
    commands = xQueueCreate(4, sizeof(Request));
    if (!commands || xTaskCreatePinnedToCore(background, "powerunit", 8192, nullptr, 1, &worker, 0) != pdPASS) {
        worker = nullptr; set_message("Power-Unit-Worker: zu wenig Speicher");
    }
}
PowerUnitSnapshot powerunit_snapshot() {
    PowerUnitSnapshot s;
    portENTER_CRITICAL(&guard); s = model; s.wanted = wanted; portEXIT_CRITICAL(&guard);
    return s;
}
void powerunit_set_wanted(bool yes) {
    portENTER_CRITICAL(&guard); wanted = yes; portEXIT_CRITICAL(&guard);
}
bool powerunit_request(Switch which, bool on) {
    if (!commands || which >= Switch::Count) return false;
    portENTER_CRITICAL(&guard);
    auto &p = model.pending[uint8_t(which)];
    bool ok = model.fresh && *model.device_id && !p.active;
    if (ok) { p.active = true; p.target = on; p.since = millis(); }
    portEXIT_CRITICAL(&guard);
    if (!ok) return false;
    Request r{command_for(which, on)};
    if (xQueueSend(commands, &r, 0) != pdTRUE) {
        portENTER_CRITICAL(&guard); model.pending[uint8_t(which)].active = false; portEXIT_CRITICAL(&guard);
        command_message("Befehlsqueue voll");
        return false;
    }
    command_message("");
    return true;
}
