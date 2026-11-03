#include "touch_release_filter.h"
#include "relay_click_filter.h"

constexpr bool held_without_new_frame() {
    TouchReleaseFilter f;
    if (!f.update(true, true, 0)) return false;
    for (unsigned t = 1; t < 5000; ++t)
        if (!f.update(t % 20 == 0, true, t)) return false;
    return f.update(true, false, 5000) &&
        (f.update(false, false, 5024) && !f.update(false, false, 5025));
}
constexpr bool dropout_and_real_release() {
    TouchReleaseFilter f;
    return f.update(true, true, 0) && f.update(true, false, 10) &&
        f.update(false, false, 25) && f.update(true, true, 30) &&
        f.update(true, false, 100) && f.update(false, false, 124) &&
        !f.update(false, false, 125);
}
constexpr bool sensor_failure_is_bounded() {
    TouchReleaseFilter f;
    return f.update(true, true, 10) && f.update(false, false, 259) &&
        !f.update(false, false, 260);
}
constexpr bool clock_rollover() {
    TouchReleaseFilter f;
    return f.update(true, true, 0xffffffe0u) && f.update(true, false, 0xfffffff0u) &&
        f.update(false, false, 8) && !f.update(false, false, 9);
}
constexpr bool reset_cancels_touch() {
    TouchReleaseFilter f; f.update(true, true, 0); f.reset();
    return !f.update(false, false, 10);
}
constexpr bool relays_debounce_independently() {
    RelayClickFilter f;
    return f.accept(0, 0) && !f.accept(0, 100) && f.accept(1, 100) &&
        f.accept(8, 100) && !f.accept(0, 599) && f.accept(0, 600) &&
        !f.accept(-1, 1000) && !f.accept(16, 1000);
}
constexpr bool relay_clock_rollover() {
    RelayClickFilter f;
    return f.accept(0, 0xffffff00u) && !f.accept(0, 343) && f.accept(0, 344);
}
static_assert(held_without_new_frame(), "a held finger must produce only one release");
static_assert(dropout_and_real_release(), "short dropout must not release the touch");
static_assert(sensor_failure_is_bounded(), "failed sensor must not leave touch stuck");
static_assert(clock_rollover(), "touch debounce must survive millis rollover");
static_assert(reset_cancels_touch(), "reset must cancel touch");
static_assert(relays_debounce_independently(), "relay debounce must protect each output independently");
static_assert(relay_clock_rollover(), "relay debounce must survive millis rollover");
