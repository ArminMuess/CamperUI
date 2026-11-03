#pragma once
#include <stdint.h>
class RelayClickFilter {
    uint32_t last[32] = {};
    bool seen[32] = {};
public:
    constexpr bool accept(int channel, uint32_t now) {
        if (channel < 0 || channel >= 32) return false;
        if (seen[channel] && now - last[channel] < 600) return false;
        seen[channel] = true; last[channel] = now;
        return true;
    }
};
