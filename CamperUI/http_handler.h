#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <Arduino.h>

void http_init();
bool http_start_task();
void http_loop();

void http_fetch_names();
bool http_publish_switch(int index, bool on);
bool http_publish_wrelay(int index, bool on);
void http_publish_dimmer(int index, int val);

void http_publish_heater_cmd(String mode, int value);
void http_publish_webasto(bool on, int temp);
void http_publish_truma_air(int temp);
void http_publish_truma_water(const char *mode);
void http_publish_truma_energy(const char *mode);
void http_publish_fridge(bool power, const char *mode, int step);
void http_calibrate_position();

#endif
