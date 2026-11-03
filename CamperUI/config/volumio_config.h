#pragma once
// Connection and presets are stored on the display, not compiled into firmware.
namespace radio_config {
constexpr unsigned POLL_MS=1000, STALE_MS=5000, COMMAND_TTL_MS=2500;
constexpr const char *PRESET_LABELS[]={"hr3","RADIO BOB!","ROCK ANTENNE"};
}
