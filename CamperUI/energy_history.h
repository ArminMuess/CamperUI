#pragma once
#include <lvgl.h>
void energy_history_init();
void energy_history_tick();
uint32_t energy_history_version();
bool energy_history_available();
bool energy_history_demo();
bool energy_history_series(int track, int hours, lv_coord_t *points, int count, int *minimum, int *maximum);
