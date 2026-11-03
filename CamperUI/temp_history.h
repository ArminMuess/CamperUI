#ifndef TEMP_HISTORY_H
#define TEMP_HISTORY_H

#include <Arduino.h>
#include <lvgl.h>

static constexpr int TEMP_HISTORY_MAX_SAMPLES = 1440; // 24 hours @ 1 sample/minute
static constexpr int TEMP_HISTORY_TRACKS = 4;        // 0: Aussen, 1: Innen, 2: Zusatz 1, 3: Zusatz 2

void temp_history_init(void);
void temp_history_tick(void);

// Fills out_points (e.g. 60 points) with scaled temp (temp * 10), or LV_CHART_POINT_NONE.
// Also outputs min and max temperature in °C over the sampled window for active points.
// Returns true if track is active and has valid points.
bool temp_history_get_series(int track_idx, int window_hours, lv_coord_t *out_points, int out_count, float *out_min, float *out_max);

// Resets / seeds the history buffer with realistic curves
void temp_history_seed_demo(void);

// Monotonically increasing revision counter (increments every time new samples are logged)
uint32_t temp_history_get_version(void);

#endif // TEMP_HISTORY_H
