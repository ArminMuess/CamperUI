#include "wifi_diagnostics.h"
#include "system_state.h"
#include <WiFi.h>
#include <esp_wifi_types.h>
#include <atomic>
#include <time.h>
#include <esp_sntp.h>
namespace {
std::atomic<unsigned> last_reason{0};
std::atomic<uint32_t> attempt_started{0};
std::atomic<uint32_t> last_reconnect_attempt{0};
std::atomic<bool> is_scanning{false};

const char *reason_text(unsigned reason) {
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND: return "Netz nicht gefunden: SSID, Reichweite und 2.4 GHz pruefen.";
        case WIFI_REASON_AUTH_FAIL: return "Anmeldung abgelehnt: Passwort oder Router-Zugriff pruefen.";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "Anmeldung ohne Antwort: Passwort oder Signal pruefen.";
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD: return "Router-Sicherheit passt nicht zur WLAN-Anmeldung.";
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD: return "WLAN-Signal zu schwach.";
        case WIFI_REASON_BEACON_TIMEOUT: return "Verbindung verloren: Signal oder Router pruefen.";
        case WIFI_REASON_ASSOC_LEAVE: return "Verbindung getrennt; neuer Verbindungsversuch.";
        case WIFI_REASON_ASSOC_EXPIRE: return "Zeitueberschreitung bei Verbindung (Signal oder Router pruefen).";
        default: return "Verbindung fehlgeschlagen: Router, Signal und Zugang pruefen.";
    }
}
}

void wifi_set_scanning(bool scanning) {
    is_scanning.store(scanning);
}

bool wifi_is_scanning() {
    return is_scanning.load();
}

void wifi_prepare_for_scan() {
    is_scanning.store(true);
    WiFi.setSleep(false);
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(false, false);
        delay(60);
    }
    WiFi.mode(WIFI_STA);
    WiFi.scanDelete();
}

void wifi_finish_scan() {
    is_scanning.store(false);
    WiFi.setSleep(true);
    if (WiFi.status() != WL_CONNECTED && !state.wifi_ssid.isEmpty()) {
        wifi_connect_configured();
    }
}

#include "improv_serial.h"

void wifi_diagnostics_init() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    improv_serial_init();
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        last_reason.store(info.wifi_sta_disconnected.reason);
        improv_serial_on_wifi_disconnected();
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
        last_reason.store(0);
        improv_serial_on_wifi_connected();
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
}

void wifi_connect_configured() {
    last_reason.store(0);
    uint32_t now = millis();
    attempt_started.store(now);
    last_reconnect_attempt.store(now);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname("esp32s3-camper-ui");
    if (state.wifi_ssid.isEmpty()) return;
    WiFi.begin(state.wifi_ssid.c_str(), state.wifi_pass.c_str());
}

void wifi_diagnostics_loop() {
    if (state.wifi_ssid.isEmpty()) return;
    if (is_scanning.load()) return;

    if (WiFi.status() == WL_CONNECTED) {
        state.wifi_connected = true;
        return;
    }

    state.wifi_connected = false;
    uint32_t now = millis();
    // Allow 15 seconds after an initial connection attempt before auto-retrying
    if (now - attempt_started.load() < 15000) return;

    // Retry every 10 seconds if disconnected
    if (now - last_reconnect_attempt.load() >= 10000) {
        last_reconnect_attempt.store(now);
        Serial.printf("[WIFI] Auto-Reconnect fuer '%s' (Grund: %u)...\n", state.wifi_ssid.c_str(), last_reason.load());
        WiFi.disconnect(false, false);
        delay(30);
        WiFi.begin(state.wifi_ssid.c_str(), state.wifi_pass.c_str());
    }
}
String wifi_connection_summary() {
    if (WiFi.status() == WL_CONNECTED) return "WLAN verbunden (" + String(WiFi.RSSI()) + " dBm)";
    if (state.wifi_ssid.isEmpty()) return "WLAN: kein Netzwerk eingestellt";
    unsigned reason = last_reason.load();
    if (reason) return "WLAN getrennt (Grund " + String(reason) + ")";
    if (WiFi.status() == WL_NO_SSID_AVAIL) return "WLAN: Netzwerk nicht gefunden";
    if (WiFi.status() == WL_CONNECT_FAILED) return "WLAN: Anmeldung fehlgeschlagen";
    return millis() - attempt_started.load() < 15000 ? "WLAN: Verbindung wird aufgebaut" : "WLAN: noch keine Verbindung";
}
String wifi_connection_details() {
    String text = wifi_connection_summary();
    if (WiFi.status() == WL_CONNECTED) {
        text += "\nNetz: " + WiFi.SSID() + "\nIP: " + WiFi.localIP().toString();
        text += "\nHostname: esp32s3-camper-ui";
    } else if (!state.wifi_ssid.isEmpty()) {
        text += "\nNetz: " + state.wifi_ssid + "\n";
        unsigned reason = last_reason.load();
        text += reason ? reason_text(reason) : "SSID, Passwort und 2.4-GHz-Netz pruefen.";
    }
    text += state.debug_mode ? "\nDaten: Dummy (WLAN-Status ist echt)" : "\nVanPi: " + String(state.vanpi_connected && WiFi.status() == WL_CONNECTED ? "erreichbar" : "noch nicht erreichbar");
    return text;
}
