#pragma once
#include <stdint.h>
#include "volumio_protocol.h"
// Radio-Pi power through the Shelly relay. None = no Shelly configured.
enum class RadioPower : uint8_t { None, Unknown, On, Off, Starting, Stopping };
struct RadioSnapshot {
    bool configured = false, online = false, pending = false;
    bool volume_available = false, muted = false;
    int volume = 0;
    uint32_t updated = 0, generation = 0;
    char service[32] = "";
    char uri[256] = "";
    char playback[12] = "";
    char artist[96] = "";
    char title[160] = "";
    char message[128] = "Radio noch nicht gestartet";
    RadioPower power = RadioPower::None;
    char power_message[96] = "";
    uint32_t power_message_at = 0;
};
bool radio_init();
RadioSnapshot radio_snapshot();
bool radio_request(radio_protocol::Action action, int value = 0);
// How often the worker asks Volumio (set by the Radio tab, UI thread):
// Visible = Radio tab on screen (1 s), Background = display on, other page (15 s),
// Off = display dark / screensaver (no polling). Switching to Visible polls at once.
enum class RadioPollMode : uint8_t { Off, Background, Visible };
void radio_set_poll_mode(RadioPollMode mode);
// Radio-Pi on: Shelly on, then wait for Volumio. Off: Volumio shutdown, wait until it
// stops answering plus a safety margin, then Shelly off. Returns false if not possible.
bool radio_power_request(bool on);
