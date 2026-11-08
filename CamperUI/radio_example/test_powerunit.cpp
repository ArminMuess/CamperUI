// Host test for powerunit_protocol.h. The sample state was produced by Pekaway's own
// pu_ble_mqtt.py (make_status) so the field layout matches the real bridge.
#include "../powerunit_protocol.h"
#include <assert.h>
#include <fstream>
#include <sstream>
#include <string>
using namespace powerunit;
static std::string sample() {
    std::ifstream f("radio_example/powerunit_state_sample.json"); std::stringstream s; s << f.rdbuf(); return s.str();
}
int main() {
    State st;
    std::string json = sample();
    assert(parse_state(json.c_str(), json.size(), st));
    assert(st.valid && st.soc == 87 && st.battery_power == -110.f && !st.battery_charging);
    assert(st.pv_power == 640.f && st.pv_charging && st.shore_power == 0.f);
    assert(st.ac_power == 5.f && fabsf(st.ac_voltage - 232.7f) < 0.01f);
    assert(st.switches[uint8_t(Switch::Inverter)] && st.switches[uint8_t(Switch::Shore)] &&
           st.switches[uint8_t(Switch::Alternator)] && !st.switches[uint8_t(Switch::Reverse)] &&
           st.switches[uint8_t(Switch::Buzzer)]);
    assert(st.remaining_minutes == 2340 && !strcmp(st.remaining_mode, "use"));
    assert(st.alert_count == 2 && !strcmp(st.alert, "Akku-Unterspannung (Warnung)"));
    assert(st.pending_count == 1 && !strcmp(st.alternator_mode, "inactive"));
    // Wrong schema, missing fields, garbage
    const char *bad[] = {"{}", "{\"schema\":2,\"battery\":{\"soc\":5},\"ac_output\":{\"enabled\":true}}",
                         "{\"schema\":1,\"battery\":{\"soc\":500},\"ac_output\":{\"enabled\":true}}",
                         "{\"schema\":1,\"battery\":{\"soc\":50}}", "nonsense"};
    for (auto *b : bad) assert(!parse_state(b, strlen(b), st) && !st.valid);
    // Null values from the bridge (e.g. current unknown) stay NaN
    const char *nulls = "{\"schema\":1,\"battery\":{\"soc\":50,\"power\":null},\"ac_output\":{\"enabled\":false,\"power\":null}}";
    assert(parse_state(nulls, strlen(nulls), st) && isnan(st.battery_power) && isnan(st.ac_power) &&
           !st.switches[uint8_t(Switch::Inverter)]);
    // Health
    bool connected = true;
    const char *h1 = "{\"connected\":false,\"state\":\"STALE\"}";
    assert(parse_health(h1, strlen(h1), connected) && !connected);
    const char *h2 = "{\"state\":1}";
    assert(!parse_health(h2, strlen(h2), connected));
    // Commands and switch mapping
    assert(!strcmp(command_text(Command::InverterOn), "ac_output_on"));
    assert(!strcmp(command_text(Command::InverterOff), "ac_output_off"));
    assert(!strcmp(command_text(Command::ShoreLow), "mains_charge_high_off"));
    assert(!strcmp(command_text(Command::AlternatorHigh), "car_charge_high"));
    for (uint8_t i = 0; i < uint8_t(Command::Count); ++i) {
        Command c = Command(i);
        assert(command_for(switch_of(c), target_of(c)) == c);
    }
    // Results
    Command c = Command::BuzzerOn; char err[48];
    const char *r1 = "{\"device_id\":\"x\",\"command\":\"ac_output_off\",\"status\":\"confirmed\",\"ok\":true}";
    assert(parse_result(r1, strlen(r1), c, err, sizeof(err)) == Result::Confirmed && c == Command::InverterOff);
    const char *r2 = "{\"command\":\"car_charge_low\",\"status\":\"error\",\"error\":\"BLE not connected\"}";
    assert(parse_result(r2, strlen(r2), c, err, sizeof(err)) == Result::Failed && c == Command::AlternatorLow &&
           !strcmp(err, "BLE not connected"));
    const char *r3 = "{\"command\":\"reverse_charge_on\",\"status\":\"timeout\"}";
    assert(parse_result(r3, strlen(r3), c, err, sizeof(err)) == Result::Failed && !strcmp(err, "timeout"));
    const char *r4 = "{\"command\":null,\"status\":\"error\"}";
    assert(parse_result(r4, strlen(r4), c, err, sizeof(err)) == Result::Unknown);
    // Topics
    char id[33], leaf[16], t[96];
    assert(split_topic("pekaway/power_unit/4142749e4256/state", id, sizeof(id), leaf, sizeof(leaf)) &&
           !strcmp(id, "4142749e4256") && !strcmp(leaf, "state"));
    assert(!split_topic("pekaway/power_unit/a/b/state", id, sizeof(id), leaf, sizeof(leaf)));
    assert(!split_topic("pekaway/power_unitx/a/state", id, sizeof(id), leaf, sizeof(leaf)));
    assert(!split_topic("pekaway/power_unit/a+b/state", id, sizeof(id), leaf, sizeof(leaf)));
    topic(t, sizeof(t), "", "state"); assert(!strcmp(t, "pekaway/power_unit/+/state"));
    topic(t, sizeof(t), "abc", "cmd"); assert(!strcmp(t, "pekaway/power_unit/abc/cmd"));
    assert(valid_device_id("4142749e4256") && !valid_device_id("") && !valid_device_id("a/b") && !valid_device_id("#"));
    // Formatting
    format_power(t, sizeof(t), NAN); assert(!strcmp(t, "--"));
    format_power(t, sizeof(t), 640); assert(!strcmp(t, "640 W"));
    format_power(t, sizeof(t), -110.4f); assert(!strcmp(t, "-110 W"));
    format_power(t, sizeof(t), 1234); assert(!strcmp(t, "1,2 kW"));
    format_power(t, sizeof(t), -2050); assert(!strcmp(t, "-2,1 kW"));
    format_duration(t, sizeof(t), 0); assert(!strcmp(t, "--"));
    format_duration(t, sizeof(t), 45); assert(!strcmp(t, "45 min"));
    format_duration(t, sizeof(t), 125); assert(!strcmp(t, "2 h 05 min"));
    format_duration(t, sizeof(t), 2340); assert(!strcmp(t, "39 h 00 min"));
    format_duration(t, sizeof(t), 4320); assert(!strcmp(t, "3 Tage"));
    // Timestamps: one taken a millisecond after `now` (millis()|1) still counts as current
    assert(within(1000, 1001, STALE_MS) && within(1000, 1000, STALE_MS) && !within(1000, 0, STALE_MS));
    assert(within(20000, 10001, STALE_MS) && !within(20000, 10000, STALE_MS));
    assert(within(5, 0xFFFFFFF0u, STALE_MS));   // millis() wraparound
    puts("powerunit protocol: ok");
}
