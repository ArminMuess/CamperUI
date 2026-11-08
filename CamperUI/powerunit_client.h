#pragma once
#include <stdint.h>
#include "powerunit_protocol.h"
// Pekaway Power Unit via the MQTT broker of the Pekaway Core.
struct PowerUnitPending { bool active = false, target = false; uint32_t since = 0; };
struct PowerUnitSnapshot {
    bool wanted = false;             // tab enabled and display on
    bool connected = false;          // MQTT session to the broker
    bool bridge_offline = false;     // bridge reports: no Bluetooth link to the Power Unit
    bool fresh = false;              // state received recently and bridge online
    powerunit::State state;
    uint32_t state_at = 0;
    char device_id[33] = "";
    char message[96] = "Power Unit noch nicht gestartet";
    PowerUnitPending pending[uint8_t(powerunit::Switch::Count)];
    char command_message[96] = "";
    uint32_t command_message_at = 0;
};
void powerunit_init();
PowerUnitSnapshot powerunit_snapshot();
// Called by the Power Unit tab (UI thread): connect only while the display is on.
void powerunit_set_wanted(bool wanted);
// Sends a switching command; false if not connected, unknown device or already pending.
bool powerunit_request(powerunit::Switch which, bool on);
