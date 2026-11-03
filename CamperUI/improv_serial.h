#ifndef IMPROV_SERIAL_H
#define IMPROV_SERIAL_H

#include <Arduino.h>
#include <vector>

void improv_serial_init();
void improv_serial_loop();
bool improv_serial_process_char(char c);

void improv_serial_on_wifi_connected();
void improv_serial_on_wifi_disconnected();

#endif // IMPROV_SERIAL_H
