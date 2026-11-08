#include "powerunit_config.h"
#include <Arduino.h>
#include <Preferences.h>
namespace {
portMUX_TYPE config_guard = portMUX_INITIALIZER_UNLOCKED;
PowerUnitConfig config;
uint32_t generation = 1;
constexpr uint32_t MAGIC = 0x50554331;   // "PUC1"
struct Stored { uint32_t magic; PowerUnitConfig data; };
}
PowerUnitEndpoint powerunit_endpoint() {
    PowerUnitEndpoint e;
    portENTER_CRITICAL(&config_guard); e.config = config; e.generation = generation; portEXIT_CRITICAL(&config_guard);
    return e;
}
void powerunit_config_load() {
    Preferences p;
    PowerUnitConfig next;
    if (p.begin("camper_pu", true)) {
        Stored stored{};
        if (p.getBytesLength("config_v1") == sizeof(stored) &&
            p.getBytes("config_v1", &stored, sizeof(stored)) == sizeof(stored) && stored.magic == MAGIC)
            next = stored.data;
        p.end();
    }
    next.host[sizeof(next.host) - 1] = 0; next.device_id[sizeof(next.device_id) - 1] = 0;
    if (!powerunit_config_valid(next)) next = PowerUnitConfig();
    portENTER_CRITICAL(&config_guard); config = next; ++generation; portEXIT_CRITICAL(&config_guard);
}
bool powerunit_config_save(const PowerUnitConfig &next) {
    if (!powerunit_config_valid(next)) return false;
    auto old = powerunit_endpoint();
    if (!memcmp(&next, &old.config, sizeof(next))) return true;
    Preferences p;
    if (!p.begin("camper_pu", false)) return false;
    Stored stored{MAGIC, next};
    bool ok = p.putBytes("config_v1", &stored, sizeof(stored)) == sizeof(stored); p.end();
    if (!ok) return false;
    portENTER_CRITICAL(&config_guard); config = next; ++generation; portEXIT_CRITICAL(&config_guard);
    return true;
}
