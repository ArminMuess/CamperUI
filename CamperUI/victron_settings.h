#pragma once
#include <ArduinoJson.h>
#include <lvgl.h>
void victron_fill_settings(JsonObject);
bool victron_apply_settings(JsonVariantConst,String &error);
void ui_victron_settings_build(lv_obj_t*,lv_event_cb_t keyboard_callback);
bool ui_victron_settings_apply(String &error);
void ui_victron_settings_update();
void ui_victron_settings_reset();
