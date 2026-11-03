#ifndef WEB_OTA_H
#define WEB_OTA_H

#include <Arduino.h>

void web_ota_init();
void web_ota_loop();
bool web_ota_is_updating();
int  web_ota_get_progress();
const char* web_ota_get_status();

#endif // WEB_OTA_H

