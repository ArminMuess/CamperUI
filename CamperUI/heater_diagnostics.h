#pragma once
#include <stdint.h>

// Volatile telemetry from VanPi /heater, not commands or persisted settings.
struct HeaterDiagnostics {
    float values[4] = {}; // V, fan rpm, fuel pump Hz, heater temperature C
    uint8_t fields = 0;
    uint32_t updated = 0;
    bool received = false, demo = false;
    static constexpr uint32_t max_age_ms = 45000;
    constexpr void begin(uint32_t now, bool simulated, bool present) {
        fields = 0; updated = now; demo = simulated; received = present;
    }
    constexpr void set(unsigned index, float value, bool valid) {
        if (index < 4 && valid) { values[index] = value; fields |= uint8_t(1U << index); }
    }
    constexpr bool fresh(uint32_t now, bool expected_demo, bool connected) const {
        return received && demo == expected_demo && (expected_demo || connected) &&
               uint32_t(now - updated) <= max_age_ms;
    }
};
