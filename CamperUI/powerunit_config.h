#pragma once
#include <stdint.h>
#include "radio_config.h"        // radio_valid_host
#include "powerunit_protocol.h"
// Pekaway Power Unit: MQTT broker on the Pekaway Core (Mosquitto, no login by default).
struct PowerUnitConfig {
    char host[64] = "";          // empty = VanPi address from the network settings
    uint16_t port = 1883;
    char device_id[33] = "";     // empty = first Power Unit that reports on the broker
};
inline bool powerunit_config_valid(const PowerUnitConfig &c) {
    return radio_valid_host(c.host) && c.port && (!*c.device_id || powerunit::valid_device_id(c.device_id));
}
struct PowerUnitEndpoint { PowerUnitConfig config; uint32_t generation = 0; };
PowerUnitEndpoint powerunit_endpoint();
void powerunit_config_load();
bool powerunit_config_save(const PowerUnitConfig &config);
