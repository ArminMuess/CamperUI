#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

void buzzer_init();
void buzzer_beep(uint16_t duration_ms = 3);
void buzzer_beep_force(uint16_t duration_ms = 3);
void buzzer_loop();

#endif // BUZZER_H
