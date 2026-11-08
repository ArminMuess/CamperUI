#pragma once
#include <lvgl.h>
class WebServer;
void powerunit_register_routes(WebServer &server);
void ui_powerunit_open_settings();
void ui_powerunit_settings_reset();
