#pragma once
#include <esp_system.h>
// Why the ESP32 started this time; shown on /status and logged at boot.
inline const char *camper_reset_reason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "Einschalten (Strom an)";
        case ESP_RST_SW: return "Neustart durch Software (z. B. Update)";
        case ESP_RST_PANIC: return "Absturz (Panic)";
        case ESP_RST_INT_WDT: return "Watchdog (Interrupt)";
        case ESP_RST_TASK_WDT: return "Watchdog (Task)";
        case ESP_RST_WDT: return "Watchdog";
        case ESP_RST_BROWNOUT: return "Unterspannung (Brownout)";
        case ESP_RST_DEEPSLEEP: return "Aufwachen aus Tiefschlaf";
        case ESP_RST_EXT: return "Reset-Taste";
        default: return "Unbekannt";
    }
}
