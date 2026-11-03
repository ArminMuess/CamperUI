#pragma once
#include <stdint.h>

// No new GT911 frame is not a release. Short dropouts cannot create more clicks.
class TouchReleaseFilter {
    bool held = false, releasing = false;
    uint32_t last_frame = 0, release_at = 0;
public:
    constexpr void reset() { held = releasing = false; }
    constexpr bool update(bool fresh, bool pressed, uint32_t now) {
        if (fresh) {
            last_frame = now;
            if (pressed) { held = true; releasing = false; }
            else if (held && !releasing) { releasing = true; release_at = now; }
            else if (held && releasing && now - release_at >= 8) { held = releasing = false; }
        }
        if (held && ((releasing && now - release_at >= 25) || now - last_frame >= 250))
            held = releasing = false;
        return held;
    }
};
