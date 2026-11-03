#include "buzzer.h"
#include "WS_CH32_IO.h"
#include "system_state.h"
#include <Wire.h>
#include <HWCDC.h>

extern TwoWire Wire;
extern HWCDC USBSerial;

static uint32_t buzzer_off_ms = 0;
static bool buzzer_is_active = false;

void buzzer_init() {
    // Ensure buzzer is muted at startup
    WS_CH32_IO::writeRegister(Wire, WS_CH32_IO::REG_OUTPUT, WS_CH32_IO::OUT_DISPLAY_ON);
}

void buzzer_beep_force(uint16_t duration_ms) {
    (void)duration_ms;
}

void buzzer_beep(uint16_t duration_ms) {
    (void)duration_ms;
}

void buzzer_loop() {
}
