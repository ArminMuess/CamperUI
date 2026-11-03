#pragma once
#include <lvgl.h>
void ui_energy_history_button(lv_obj_t *card, bool solar);
void ui_energy_history_destroy();
bool ui_energy_history_active();
void ui_energy_history_update();
