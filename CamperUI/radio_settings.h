#pragma once
#include <lvgl.h>
class WebServer;
void radio_register_routes(WebServer &server);
void ui_radio_open_settings();
void ui_radio_settings_reset();
