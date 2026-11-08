#include "crash_report.h"
#include "debug_log.h"
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <string.h>
namespace {
constexpr uint32_t MAGIC = 0x43524153;   // "CRAS"
struct Stored {
    uint32_t magic;
    int core;
    char reason[48];
    uint32_t pc;
    uint32_t length;
    uint32_t backtrace[16];
};
RTC_NOINIT_ATTR Stored stored;
char summary[400] = "";
// Runs inside the panic handler: no allocation, no logging, plain copies only.
void IRAM_ATTR on_panic(arduino_panic_info_t *info, void *) {
    stored.core = info->core;
    const char *r = info->reason ? info->reason : "?";
    size_t i = 0;
    for (; i < sizeof(stored.reason) - 1 && r[i]; ++i) stored.reason[i] = r[i];
    stored.reason[i] = 0;
    stored.pc = uint32_t(info->pc);
    stored.length = info->backtrace_len < 16 ? info->backtrace_len : 16;
    for (uint32_t k = 0; k < stored.length; ++k) stored.backtrace[k] = info->backtrace[k];
    stored.magic = MAGIC;
}
}  // namespace
void crash_report_init() {
    if (stored.magic == MAGIC && stored.length <= 16) {
        stored.reason[sizeof(stored.reason) - 1] = 0;
        int n = snprintf(summary, sizeof(summary), "Kern %d: %s | PC 0x%08lx | Backtrace:", stored.core, stored.reason,
                         (unsigned long)stored.pc);
        for (uint32_t k = 0; k < stored.length && n > 0 && size_t(n) < sizeof(summary) - 12; ++k)
            n += snprintf(summary + n, sizeof(summary) - n, " 0x%08lx", (unsigned long)stored.backtrace[k]);
        Serial.printf("[CRASH] %s\n", summary);
    }
    stored.magic = 0;
    set_arduino_panic_handler(on_panic, nullptr);
}
const char *crash_report_summary() { return summary; }
