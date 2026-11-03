#include "temp_history.h"
#include "system_state.h"
#include <math.h>

static int16_t (*history_samples)[TEMP_HISTORY_TRACKS] = nullptr;
static int head_idx = 0;
static int sample_count = 0;
static uint32_t last_sample_millis = 0;
static uint32_t history_version = 1;

uint32_t temp_history_get_version(void) {
    return history_version;
}

void temp_history_seed_demo(void) {
    if (!history_samples) return;
    for (int m = 0; m < TEMP_HISTORY_MAX_SAMPLES; ++m) {
        // Aussen: 24-hour curve with realistic diurnal variation
        float t_out = 14.5f + 5.5f * sinf((float)m * (2.0f * (float)M_PI / 1440.0f)) + 0.2f * sinf((float)m * 0.15f);
        // Innen: stable around 21-23 °C
        float t_in = 21.8f + 1.2f * sinf(((float)m + 240.0f) * (2.0f * (float)M_PI / 1440.0f));
        // Track 2 (z.B. Kühlbox): cyclic cooling pattern 4.5 - 7.5 °C
        float t_c1 = 6.0f + 1.6f * sinf((float)m * (2.0f * (float)M_PI / 60.0f));
        // Track 3 (z.B. Ruuvi Schlafbereich): 18.5 - 20.5 °C
        float t_c2 = 19.5f + 0.9f * cosf((float)m * (2.0f * (float)M_PI / 720.0f));

        history_samples[m][0] = (int16_t)roundf(t_out * 10.0f);
        history_samples[m][1] = (int16_t)roundf(t_in * 10.0f);
        history_samples[m][2] = (int16_t)roundf(t_c1 * 10.0f);
        history_samples[m][3] = (int16_t)roundf(t_c2 * 10.0f);
    }
    head_idx = 0;
    sample_count = TEMP_HISTORY_MAX_SAMPLES;
    last_sample_millis = millis();
    history_version++;
}

void temp_history_init(void) {
    if (!history_samples) {
        history_samples = (int16_t (*)[TEMP_HISTORY_TRACKS])heap_caps_malloc(
            sizeof(int16_t) * TEMP_HISTORY_MAX_SAMPLES * TEMP_HISTORY_TRACKS, MALLOC_CAP_SPIRAM);
        if (!history_samples) {
            history_samples = (int16_t (*)[TEMP_HISTORY_TRACKS])malloc(
                sizeof(int16_t) * TEMP_HISTORY_MAX_SAMPLES * TEMP_HISTORY_TRACKS);
        }
    }
    if (!history_samples) return;

    for (int i = 0; i < TEMP_HISTORY_MAX_SAMPLES; ++i) {
        for (int t = 0; t < TEMP_HISTORY_TRACKS; ++t) {
            history_samples[i][t] = INT16_MIN;
        }
    }
    head_idx = 0;
    sample_count = 0;
    last_sample_millis = 0;
    history_version++;

    // Seed plausible curves immediately so the charts look alive right from the start
    temp_history_seed_demo();
}

static inline int16_t get_track_current_raw(int track_idx) {
    if (track_idx == 0) {
        if (isfinite(state.outdoor_temp) && state.outdoor_temp > -40.0f && state.outdoor_temp < 80.0f) {
            return (int16_t)roundf(state.outdoor_temp * 10.0f);
        }
        return INT16_MIN;
    }
    if (track_idx == 1) {
        if (isfinite(state.indoor_temp) && state.indoor_temp > -40.0f && state.indoor_temp < 80.0f) {
            return (int16_t)roundf(state.indoor_temp * 10.0f);
        }
        return INT16_MIN;
    }
    int custom_slot = track_idx - 2;
    if (custom_slot >= 0 && custom_slot < 2) {
        int sensor_idx = state.temps_custom_sensor[custom_slot];
        if (sensor_idx >= 0 && sensor_idx < TEMP_SOURCE_COUNT) {
            float val = state.temp_sensors[sensor_idx];
            if (isfinite(val) && val > -40.0f && val < 80.0f) {
                return (int16_t)roundf(val * 10.0f);
            }
        }
    }
    return INT16_MIN;
}

void temp_history_tick(void) {
    if (!history_samples) return;
    uint32_t now = millis();
    // Sample once every 60 seconds (60000 ms)
    if (last_sample_millis != 0 && (now - last_sample_millis < 60000)) {
        return;
    }
    last_sample_millis = now;

    // Record sample at head_idx
    for (int t = 0; t < TEMP_HISTORY_TRACKS; ++t) {
        int16_t v = get_track_current_raw(t);
        if (v != INT16_MIN) {
            history_samples[head_idx][t] = v;
        } else if (sample_count > 0) {
            // Keep previous value if temporarily missing to avoid dropouts
            int prev = (head_idx - 1 + TEMP_HISTORY_MAX_SAMPLES) % TEMP_HISTORY_MAX_SAMPLES;
            history_samples[head_idx][t] = history_samples[prev][t];
        } else {
            history_samples[head_idx][t] = INT16_MIN;
        }
    }

    head_idx = (head_idx + 1) % TEMP_HISTORY_MAX_SAMPLES;
    if (sample_count < TEMP_HISTORY_MAX_SAMPLES) {
        sample_count++;
    }
    history_version++;
}

bool temp_history_get_series(int track_idx, int window_hours, lv_coord_t *out_points, int out_count, float *out_min, float *out_max) {
    if (!history_samples || !out_points || out_count <= 0 || track_idx < 0 || track_idx >= TEMP_HISTORY_TRACKS) {
        return false;
    }

    // Check if custom track is disabled
    if (track_idx >= 2) {
        int custom_slot = track_idx - 2;
        if (state.temps_custom_sensor[custom_slot] < 0 || state.temps_custom_sensor[custom_slot] >= TEMP_SOURCE_COUNT) {
            for (int i = 0; i < out_count; ++i) {
                out_points[i] = LV_CHART_POINT_NONE;
            }
            if (out_min) *out_min = 0.0f;
            if (out_max) *out_max = 0.0f;
            return false;
        }
    }

    int total_minutes = constrain(window_hours, 1, 24) * 60;
    float step = (float)(total_minutes - 1) / (float)(out_count - 1);

    int16_t min_raw = 32767;
    int16_t max_raw = -32768;
    bool has_valid = false;

    for (int i = 0; i < out_count; ++i) {
        // i = 0 is oldest (total_minutes ago), i = out_count - 1 is newest (now)
        float minutes_ago = (float)(out_count - 1 - i) * step;
        int offset = (int)roundf(minutes_ago);
        int buf_idx = (head_idx - 1 - offset + TEMP_HISTORY_MAX_SAMPLES * 2) % TEMP_HISTORY_MAX_SAMPLES;

        int16_t val = history_samples[buf_idx][track_idx];
        if (val == INT16_MIN) {
            out_points[i] = LV_CHART_POINT_NONE;
        } else {
            out_points[i] = (lv_coord_t)val;
            if (val < min_raw) min_raw = val;
            if (val > max_raw) max_raw = val;
            has_valid = true;
        }
    }

    if (has_valid) {
        if (out_min) *out_min = (float)min_raw / 10.0f;
        if (out_max) *out_max = (float)max_raw / 10.0f;
        return true;
    } else {
        if (out_min) *out_min = 0.0f;
        if (out_max) *out_max = 0.0f;
        return false;
    }
}
