#include "heater_diagnostics.h"
constexpr bool missing_and_zero() {
    HeaterDiagnostics d; d.begin(100, false, true);
    d.set(0, 13.0f, true); d.set(1, 0, true); d.set(2, 1.5f, true); d.set(3, 144, true);
    if (d.fields != 15 || d.values[1] != 0 || !d.fresh(100, false, true)) return false;
    d.begin(200, false, true); d.set(0, 13, true); d.set(1, 0, false);
    return d.fields == 1; // A new partial reply must invalidate previously available fields.
}
constexpr bool freshness_and_sources() {
    HeaterDiagnostics d;
    if (d.fresh(0, false, true)) return false;
    d.begin(100, false, true);
    if (!d.fresh(45100, false, true) || d.fresh(45101, false, true)) return false;
    if (d.fresh(100, true, true) || d.fresh(100, false, false)) return false;
    d.begin(100, true, true);
    if (!d.fresh(100, true, false) || d.fresh(100, false, true)) return false;
    d.begin(0xfffffff0U, false, true);
    return d.fresh(10, false, true) && !d.fresh(45000, false, true);
}
static_assert(missing_and_zero(), "Missing telemetry must not appear as measured zero");
static_assert(freshness_and_sources(), "Hide stale, disconnected and wrong-source telemetry, including millis wrap");
