#include "ui_temps.h"
#include "ui_main.h"
#include "ui_chart_style.h"
#include "temp_history.h"
#include "buzzer.h"
#include <stdio.h>
#include <math.h>

static lv_obj_t *root_page = nullptr;
static lv_obj_t *chart = nullptr;
static lv_obj_t *chart_card = nullptr;
static lv_chart_series_t *ser_outdoor = nullptr;
static lv_chart_series_t *ser_indoor = nullptr;
static lv_chart_series_t *ser_custom1 = nullptr;
static lv_chart_series_t *ser_custom2 = nullptr;

static lv_obj_t *dd_win = nullptr;

static lv_obj_t *cards[4] = {};
static lv_obj_t *accents[4] = {};
static lv_obj_t *lbl_names[4] = {};
static lv_obj_t *lbl_vals[4] = {};
static lv_obj_t *lbl_hum[4] = {};
static lv_obj_t *lbl_max[4] = {};
static lv_obj_t *lbl_min[4] = {};

static lv_obj_t *lbl_scale_top = nullptr;
static lv_obj_t *lbl_scale_bot = nullptr;
static lv_obj_t *lbl_axis_start = nullptr;
static lv_obj_t *lbl_axis_mid = nullptr;
static lv_obj_t *lbl_axis_now = nullptr;

static constexpr int CHART_POINTS = 60;
static bool plot_track_visible[4] = {true, true, true, true};

static const uint32_t TRACK_COLORS_DARK[4] = {
    0x38bdf8, // Aussen: Sky Blue
    0x34d399, // Innen: Emerald Green
    0xfbbf24, // Zusatz 1: Amber
    0xc084fc  // Zusatz 2: Purple
};

static const uint32_t TRACK_COLORS_LIGHT[4] = {
    0x0284c7, // Aussen: Darker Blue
    0x059669, // Innen: Forest Green
    0xd97706, // Zusatz 1: Dark Amber
    0x9333ea  // Zusatz 2: Purple
};

static inline lv_color_t track_color(int i) {
    return lv_color_hex(state.dark_mode ? TRACK_COLORS_DARK[i] : TRACK_COLORS_LIGHT[i]);
}

// Animation state
static bool temps_animating = false;
static int32_t anim_revealed_points = CHART_POINTS;

static void update_axis_labels(void) {
    int win = state.temps_history_window;
    if (lbl_axis_start) {
        if (win == 1)       ui_label_set_text_if_changed(lbl_axis_start, "-1 Std.");
        else if (win == 6)  ui_label_set_text_if_changed(lbl_axis_start, "-6 Std.");
        else if (win == 12) ui_label_set_text_if_changed(lbl_axis_start, "-12 Std.");
        else                ui_label_set_text_if_changed(lbl_axis_start, "-24 Std.");
    }
    if (lbl_axis_mid) {
        if (win == 1)       ui_label_set_text_if_changed(lbl_axis_mid, "-30 Min.");
        else if (win == 6)  ui_label_set_text_if_changed(lbl_axis_mid, "-3 Std.");
        else if (win == 12) ui_label_set_text_if_changed(lbl_axis_mid, "-6 Std.");
        else                ui_label_set_text_if_changed(lbl_axis_mid, "-12 Std.");
    }
}

static void dd_win_event_cb(lv_event_t *e) {
    lv_obj_t *dd = lv_event_get_target(e);
    buzzer_beep(3);
    int sel = lv_dropdown_get_selected(dd);
    int win = 1;
    if (sel == 1) win = 6;
    else if (sel == 2) win = 12;
    else if (sel == 3) win = 24;

    state.temps_history_window = win;
    update_axis_labels();
    ui_trigger_temps_anim();
}

static int last_layout_custom_count = -1;
static int last_layout_c1 = -1;
static int last_layout_c2 = -1;
static uint32_t last_rendered_hist_ver = 0xFFFFFFFF;
static int last_rendered_win = -1;
static int last_rendered_points_to_show = -1;
static int8_t last_card_hum_mode[4] = {-1, -1, -1, -1};
static lv_coord_t cached_points[4][CHART_POINTS];
static bool cached_track_valid[4] = {false, false, false, false};

static void temps_anim_cb(void *var, int32_t val) {
    (void)var;
    int target_pts = constrain((int)val, 2, CHART_POINTS);
    if (target_pts == last_rendered_points_to_show && last_rendered_win != -1) {
        anim_revealed_points = target_pts;
        return;
    }
    anim_revealed_points = target_pts;

    // Fast path: if series data is already cached for current window/version, only reveal new points
    int win = constrain(state.temps_history_window, 1, 24);
    uint32_t hist_ver = temp_history_get_version();
    if (chart && win == last_rendered_win && hist_ver == last_rendered_hist_ver && last_rendered_points_to_show >= 2) {
        lv_chart_series_t *sers[4] = {ser_outdoor, ser_indoor, ser_custom1, ser_custom2};
        int start_idx = (target_pts > last_rendered_points_to_show) ? last_rendered_points_to_show : 0;
        for (int t = 0; t < 4; ++t) {
            if (!sers[t]) continue;
            if (cached_track_valid[t] && plot_track_visible[t]) {
                for (int i = start_idx; i < target_pts; ++i) {
                    lv_chart_set_value_by_id(chart, sers[t], i, cached_points[t][i]);
                }
                for (int i = target_pts; i < last_rendered_points_to_show; ++i) {
                    lv_chart_set_value_by_id(chart, sers[t], i, LV_CHART_POINT_NONE);
                }
            }
        }
        last_rendered_points_to_show = target_pts;
        lv_chart_refresh(chart);
        return;
    }

    StateLockGuard lock;
    ui_update_temps_tab();
}

static void temps_anim_ready_cb(lv_anim_t *a) {
    (void)a;
    temps_animating = false;
    anim_revealed_points = CHART_POINTS;
    StateLockGuard lock;
    ui_update_temps_tab();
}

void ui_trigger_temps_anim(void) {
    if (!chart) return;
    lv_anim_del(chart, temps_anim_cb);
    temps_animating = true;
    anim_revealed_points = 2;
    last_rendered_points_to_show = -1;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, chart);
    lv_anim_set_exec_cb(&a, temps_anim_cb);
    lv_anim_set_ready_cb(&a, temps_anim_ready_cb);
    lv_anim_set_values(&a, 2, CHART_POINTS);
    lv_anim_set_time(&a, 750);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void ui_temps_reset(void) {
    if (chart) lv_anim_del(chart, temps_anim_cb);
    temps_animating = false;
    anim_revealed_points = CHART_POINTS;
    root_page = nullptr;
    chart_card = nullptr;
    chart = nullptr;
    dd_win = nullptr;
    lbl_scale_top = nullptr;
    lbl_scale_bot = nullptr;
    lbl_axis_start = nullptr;
    lbl_axis_mid = nullptr;
    lbl_axis_now = nullptr;
    ser_outdoor = ser_indoor = ser_custom1 = ser_custom2 = nullptr;
    for (int i = 0; i < 4; ++i) {
        cards[i] = nullptr;
        accents[i] = nullptr;
        lbl_names[i] = nullptr;
        lbl_vals[i] = nullptr;
        lbl_hum[i] = nullptr;
        lbl_max[i] = nullptr;
        lbl_min[i] = nullptr;
        last_card_hum_mode[i] = -1;
        cached_track_valid[i] = false;
    }
}

static void card_clicked_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < 4) {
        plot_track_visible[idx] = !plot_track_visible[idx];
        last_rendered_hist_ver = 0xFFFFFFFF; // force curve re-render
        ui_update_temps_tab();
    }
}

void ui_build_temps(lv_obj_t *parent) {
    ui_temps_reset();
    last_layout_custom_count = -1;
    last_layout_c1 = -1;
    last_layout_c2 = -1;
    last_rendered_hist_ver = 0xFFFFFFFF;
    last_rendered_win = -1;
    last_rendered_points_to_show = -1;

    root_page = parent;
    ui_setup_tab_page(parent);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // ==========================================
    // 1. Metric Cards (60px height: Name, Temp, Humidity, Min/Max)
    // Tapping any card toggles its curve visibility in the plot.
    // ==========================================
    for (int i = 0; i < 4; ++i) {
        cards[i] = lv_obj_create(parent);
        lv_obj_set_size(cards[i], 216, 60);
        lv_obj_set_style_bg_color(cards[i], ui_theme_card(), 0);
        lv_obj_set_style_border_color(cards[i], ui_theme_border(), 0);
        lv_obj_set_style_border_width(cards[i], 1, 0);
        lv_obj_set_style_radius(cards[i], 12, 0);
        lv_obj_set_style_pad_all(cards[i], 0, 0);
        lv_obj_clear_flag(cards[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(cards[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(cards[i], card_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        // Colored accent bar
        accents[i] = lv_obj_create(cards[i]);
        lv_obj_remove_style_all(accents[i]);
        lv_obj_set_size(accents[i], 4, 44);
        lv_obj_set_pos(accents[i], 5, 8);
        lv_obj_set_style_radius(accents[i], 2, 0);
        lv_obj_set_style_bg_color(accents[i], track_color(i), 0);
        lv_obj_set_style_bg_opa(accents[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(accents[i], LV_OBJ_FLAG_CLICKABLE);

        // Sensor name (Top-left, 14px font)
        lbl_names[i] = lv_label_create(cards[i]);
        lv_obj_set_pos(lbl_names[i], 16, 5);
        lv_obj_set_width(lbl_names[i], 108);
        lv_label_set_long_mode(lbl_names[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(lbl_names[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_names[i], ui_theme_text(), 0);

        // Current value (Bottom-left: 24px font highlighted in track color)
        lbl_vals[i] = lv_label_create(cards[i]);
        lv_obj_set_pos(lbl_vals[i], 16, 24);
        lv_obj_set_style_text_font(lbl_vals[i], &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl_vals[i], track_color(i), 0);
        lv_label_set_text(lbl_vals[i], "--");

        // Humidity label (Top-right, 12px font, Sky Blue)
        lbl_hum[i] = lv_label_create(cards[i]);
        lv_obj_align(lbl_hum[i], LV_ALIGN_TOP_RIGHT, -10, 5);
        lv_obj_set_style_text_font(lbl_hum[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_hum[i], lv_color_hex(0x38bdf8), 0);
        lv_label_set_text(lbl_hum[i], "");

        // Max sub-label (Right column)
        lbl_max[i] = lv_label_create(cards[i]);
        lv_obj_align(lbl_max[i], LV_ALIGN_TOP_RIGHT, -10, 23);
        lv_obj_set_style_text_font(lbl_max[i], &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_max[i], ui_theme_muted(), 0);
        lv_label_set_text(lbl_max[i], "Max: --");

        // Min sub-label (Right column)
        lbl_min[i] = lv_label_create(cards[i]);
        lv_obj_align(lbl_min[i], LV_ALIGN_TOP_RIGHT, -10, 39);
        lv_obj_set_style_text_font(lbl_min[i], &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_min[i], ui_theme_muted(), 0);
        lv_label_set_text(lbl_min[i], "Min: --");
    }

    lv_label_set_text(lbl_names[0], "Außen");
    lv_label_set_text(lbl_names[1], "Innen");
    lv_label_set_text(lbl_names[2], "Zusatz 1");
    lv_label_set_text(lbl_names[3], "Zusatz 2");

    // ==========================================
    // 2. Chart Card (y = 134, h = 224px, w = 440px)
    // Scale labels & time range dropdown on top, full-width chart below.
    // ==========================================
    chart_card = lv_obj_create(parent);
    lv_obj_set_pos(chart_card, 0, 134);
    lv_obj_set_size(chart_card, 440, 224);
    ui_chart_card_style(chart_card);

    // Scale header labels
    lbl_scale_top = lv_label_create(chart_card);
    lv_obj_set_pos(lbl_scale_top, 10, 7);
    lv_obj_set_style_text_font(lbl_scale_top, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_scale_top, ui_theme_muted(), 0);
    lv_label_set_text(lbl_scale_top, "Max: -- C");

    lbl_scale_bot = lv_label_create(chart_card);
    lv_obj_set_pos(lbl_scale_bot, 110, 7);
    lv_obj_set_style_text_font(lbl_scale_bot, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_scale_bot, ui_theme_muted(), 0);
    lv_label_set_text(lbl_scale_bot, "Min: -- C");

    // Time Range Dropdown in top-right corner of the chart area
    dd_win = lv_dropdown_create(chart_card);
    lv_dropdown_set_options(dd_win, "1h\n6h\n12h\n24h");
    lv_obj_set_size(dd_win, 76, 28);
    lv_obj_align(dd_win, LV_ALIGN_TOP_RIGHT, -6, 2);
    lv_obj_set_style_radius(dd_win, 8, 0);
    lv_obj_set_style_bg_color(dd_win, ui_theme_card(), 0);
    lv_obj_set_style_border_color(dd_win, ui_theme_border(), 0);
    lv_obj_set_style_border_width(dd_win, 1, 0);
    lv_obj_set_style_text_color(dd_win, ui_theme_text(), 0);
    lv_obj_set_style_text_font(dd_win, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_top(dd_win, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(dd_win, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_left(dd_win, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(dd_win, 22, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dd_win, 0, 0);

    int cur_sel = 0;
    if (state.temps_history_window == 6) cur_sel = 1;
    else if (state.temps_history_window == 12) cur_sel = 2;
    else if (state.temps_history_window == 24) cur_sel = 3;
    lv_dropdown_set_selected(dd_win, cur_sel);

    lv_obj_t *list = lv_dropdown_get_list(dd_win);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_all(list, 6, 0);
    lv_obj_set_style_bg_color(list, ui_theme_card(), 0);
    lv_obj_set_style_border_color(list, ui_theme_border(), 0);
    lv_obj_set_style_border_width(list, 1, 0);
    lv_obj_set_style_radius(list, 8, 0);
    lv_obj_add_event_cb(dd_win, dd_win_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Full-Width Line Chart (w = 386, h = 156)
    chart = lv_chart_create(chart_card);
    lv_obj_set_pos(chart, 44, 34);
    lv_obj_set_size(chart, 386, 156);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, CHART_POINTS);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 0, 4, 1, true, 42);
    lv_obj_set_style_text_font(chart, &lv_font_montserrat_12, LV_PART_TICKS);
    lv_obj_set_style_text_color(chart, ui_theme_muted(), LV_PART_TICKS);
    lv_obj_add_event_cb(chart, [](lv_event_t *event) {
        lv_obj_draw_part_dsc_t *part = lv_event_get_draw_part_dsc(event);
        if (part && part->type == LV_CHART_DRAW_PART_TICK_LABEL &&
            part->id == LV_CHART_AXIS_PRIMARY_Y && part->text) {
            snprintf(part->text, part->text_length, "%.1f", part->value / 10.0f);
        }
    }, LV_EVENT_DRAW_PART_BEGIN, nullptr);

    ui_chart_grid_style(chart);
    lv_chart_set_div_line_count(chart, 4, 0);

    // Add 4 Series with track colors
    ser_outdoor = lv_chart_add_series(chart, track_color(0), LV_CHART_AXIS_PRIMARY_Y);
    ser_indoor  = lv_chart_add_series(chart, track_color(1), LV_CHART_AXIS_PRIMARY_Y);
    ser_custom1 = lv_chart_add_series(chart, track_color(2), LV_CHART_AXIS_PRIMARY_Y);
    ser_custom2 = lv_chart_add_series(chart, track_color(3), LV_CHART_AXIS_PRIMARY_Y);

    // Time Axis Footer Labels (under the chart)
    lbl_axis_start = lv_label_create(chart_card);
    lv_obj_set_pos(lbl_axis_start, 44, 198);
    lv_obj_set_style_text_font(lbl_axis_start, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_axis_start, ui_theme_muted(), 0);
    lv_label_set_text(lbl_axis_start, "-1 Std.");

    lbl_axis_mid = lv_label_create(chart_card);
    lv_obj_set_pos(lbl_axis_mid, 215, 198);
    lv_obj_set_style_text_font(lbl_axis_mid, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_axis_mid, ui_theme_muted(), 0);
    lv_label_set_text(lbl_axis_mid, "-30 Min.");

    lbl_axis_now = lv_label_create(chart_card);
    lv_obj_set_pos(lbl_axis_now, 390, 198);
    lv_obj_set_style_text_font(lbl_axis_now, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_axis_now, ui_theme_muted(), 0);
    lv_label_set_text(lbl_axis_now, "Jetzt");

    update_axis_labels();

    // Initial render and launch reveal animation
    ui_update_temps_tab();
    ui_trigger_temps_anim();
}

void ui_update_temps_tab(void) {
    if (!root_page || !chart || !chart_card) return;

    bool c1_active = (state.temps_custom_sensor[0] >= 0 && state.temps_custom_sensor[0] < TEMP_SOURCE_COUNT);
    bool c2_active = (state.temps_custom_sensor[1] >= 0 && state.temps_custom_sensor[1] < TEMP_SOURCE_COUNT);
    int custom_count = (c1_active ? 1 : 0) + (c2_active ? 1 : 0);

    // Dynamic card & chart layout depending on number of active sensors
    if (custom_count != last_layout_custom_count ||
        state.temps_custom_sensor[0] != last_layout_c1 ||
        state.temps_custom_sensor[1] != last_layout_c2) {
        last_layout_custom_count = custom_count;
        last_layout_c1 = state.temps_custom_sensor[0];
        last_layout_c2 = state.temps_custom_sensor[1];
        for (int i = 0; i < 4; ++i) last_card_hum_mode[i] = -1;

        if (custom_count == 0) {
            // Only 2 cards: Outdoor and Indoor side-by-side
            lv_obj_clear_flag(cards[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cards[0], 0, 0);
            lv_obj_set_size(cards[0], 216, 60);
            lv_obj_set_width(lbl_names[0], 108);

            lv_obj_clear_flag(cards[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cards[1], 224, 0);
            lv_obj_set_size(cards[1], 216, 60);
            lv_obj_set_width(lbl_names[1], 108);

            lv_obj_add_flag(cards[2], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(cards[3], LV_OBJ_FLAG_HIDDEN);

            // Expand chart card to 290px height
            lv_obj_set_pos(chart_card, 0, 68);
            lv_obj_set_size(chart_card, 440, 290);
            lv_obj_set_pos(chart, 44, 34);
            lv_obj_set_size(chart, 386, 222);
            if (lbl_axis_start) lv_obj_set_pos(lbl_axis_start, 44, 264);
            if (lbl_axis_mid)   lv_obj_set_pos(lbl_axis_mid, 215, 264);
            if (lbl_axis_now)   lv_obj_set_pos(lbl_axis_now, 390, 264);
        } else if (custom_count == 1) {
            // 3 cards: Outdoor and Indoor on row 1, single custom sensor on row 2 (full width)
            lv_obj_clear_flag(cards[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cards[0], 0, 0);
            lv_obj_set_size(cards[0], 216, 60);
            lv_obj_set_width(lbl_names[0], 108);

            lv_obj_clear_flag(cards[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cards[1], 224, 0);
            lv_obj_set_size(cards[1], 216, 60);
            lv_obj_set_width(lbl_names[1], 108);

            int active_idx = c1_active ? 2 : 3;
            int hidden_idx = c1_active ? 3 : 2;

            lv_obj_add_flag(cards[hidden_idx], LV_OBJ_FLAG_HIDDEN);

            lv_obj_clear_flag(cards[active_idx], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cards[active_idx], 0, 66);
            lv_obj_set_size(cards[active_idx], 440, 60);
            lv_obj_set_width(lbl_names[active_idx], 260);

            lv_obj_set_pos(chart_card, 0, 134);
            lv_obj_set_size(chart_card, 440, 224);
            lv_obj_set_pos(chart, 44, 34);
            lv_obj_set_size(chart, 386, 156);
            if (lbl_axis_start) lv_obj_set_pos(lbl_axis_start, 44, 198);
            if (lbl_axis_mid)   lv_obj_set_pos(lbl_axis_mid, 215, 198);
            if (lbl_axis_now)   lv_obj_set_pos(lbl_axis_now, 390, 198);
        } else {
            // 4 cards: 2x2 grid
            lv_obj_clear_flag(cards[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cards[0], 0, 0);
            lv_obj_set_size(cards[0], 216, 60);
            lv_obj_set_width(lbl_names[0], 108);

            lv_obj_clear_flag(cards[1], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cards[1], 224, 0);
            lv_obj_set_size(cards[1], 216, 60);
            lv_obj_set_width(lbl_names[1], 108);

            lv_obj_clear_flag(cards[2], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cards[2], 0, 66);
            lv_obj_set_size(cards[2], 216, 60);
            lv_obj_set_width(lbl_names[2], 108);

            lv_obj_clear_flag(cards[3], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cards[3], 224, 66);
            lv_obj_set_size(cards[3], 216, 60);
            lv_obj_set_width(lbl_names[3], 108);

            lv_obj_set_pos(chart_card, 0, 134);
            lv_obj_set_size(chart_card, 440, 224);
            lv_obj_set_pos(chart, 44, 34);
            lv_obj_set_size(chart, 386, 156);
            if (lbl_axis_start) lv_obj_set_pos(lbl_axis_start, 44, 198);
            if (lbl_axis_mid)   lv_obj_set_pos(lbl_axis_mid, 215, 198);
            if (lbl_axis_now)   lv_obj_set_pos(lbl_axis_now, 390, 198);
        }
    }

    // Sync dropdown selection if not open
    if (dd_win && !lv_dropdown_is_open(dd_win)) {
        int expected_sel = 0;
        if (state.temps_history_window == 6) expected_sel = 1;
        else if (state.temps_history_window == 12) expected_sel = 2;
        else if (state.temps_history_window == 24) expected_sel = 3;
        if ((int)lv_dropdown_get_selected(dd_win) != expected_sel) {
            lv_dropdown_set_selected(dd_win, expected_sel);
            update_axis_labels();
        }
    }

    char buf[48];

    // 1. Live Current Values, Humidity, Names & Active/Dimmed Styling
    int s_idx[4] = {
        constrain(state.outdoor_temp_sensor, 0, TEMP_SOURCE_COUNT - 1),
        constrain(state.indoor_temp_sensor, 0, TEMP_SOURCE_COUNT - 1),
        state.temps_custom_sensor[0],
        state.temps_custom_sensor[1]
    };

    // Card 0: Außen
    if (isfinite(state.outdoor_temp) && state.outdoor_temp > -40.0f && state.outdoor_temp < 80.0f) {
        snprintf(buf, sizeof(buf), "%.1f C", state.outdoor_temp);
        ui_label_set_text_if_changed(lbl_vals[0], buf);
    } else {
        ui_label_set_text_if_changed(lbl_vals[0], "--");
    }
    ui_label_set_text_if_changed(lbl_names[0], "Außen");

    // Card 1: Innen
    if (isfinite(state.indoor_temp) && state.indoor_temp > -40.0f && state.indoor_temp < 80.0f) {
        snprintf(buf, sizeof(buf), "%.1f C", state.indoor_temp);
        ui_label_set_text_if_changed(lbl_vals[1], buf);
    } else {
        ui_label_set_text_if_changed(lbl_vals[1], "--");
    }
    ui_label_set_text_if_changed(lbl_names[1], "Innen");

    // Card 2: Zusatz 1
    int c1_idx = s_idx[2];
    if (c1_active) {
        float c1_val = state.temp_sensors[c1_idx];
        if (isfinite(c1_val) && c1_val > -40.0f && c1_val < 80.0f) {
            snprintf(buf, sizeof(buf), "%.1f C", c1_val);
            ui_label_set_text_if_changed(lbl_vals[2], buf);
        } else {
            ui_label_set_text_if_changed(lbl_vals[2], "--");
        }
        const char *nm = state.temp_sensor_names[c1_idx].length() ? state.temp_sensor_names[c1_idx].c_str() : (c1_idx < 4 ? "Temp 3" : "Ruuvi");
        ui_label_set_text_if_changed(lbl_names[2], nm);
    } else {
        ui_label_set_text_if_changed(lbl_vals[2], "--");
    }

    // Card 3: Zusatz 2
    int c2_idx = s_idx[3];
    if (c2_active) {
        float c2_val = state.temp_sensors[c2_idx];
        if (isfinite(c2_val) && c2_val > -40.0f && c2_val < 80.0f) {
            snprintf(buf, sizeof(buf), "%.1f C", c2_val);
            ui_label_set_text_if_changed(lbl_vals[3], buf);
        } else {
            ui_label_set_text_if_changed(lbl_vals[3], "--");
        }
        const char *nm = state.temp_sensor_names[c2_idx].length() ? state.temp_sensor_names[c2_idx].c_str() : (c2_idx < 4 ? "Temp 4" : "Ruuvi");
        ui_label_set_text_if_changed(lbl_names[3], nm);
    } else {
        ui_label_set_text_if_changed(lbl_vals[3], "--");
    }

    // Per-card Humidity & Visual State (Active vs Muted/Toggled-Off)
    for (int i = 0; i < 4; ++i) {
        int idx = s_idx[i];
        float hum = (idx >= 0 && idx < TEMP_SOURCE_COUNT) ? state.temp_humidity[idx] : 0.0f;
        int align_x = (custom_count == 1 && i == (c1_active ? 2 : 3)) ? -12 : -10;
        bool has_hum = (hum > 0.5f && hum <= 100.0f);
        int8_t hum_mode = (int8_t)((has_hum ? 1 : 0) | ((align_x == -12) ? 2 : 0));

        if (has_hum) {
            snprintf(buf, sizeof(buf), "%.1f %%", hum);
            ui_label_set_text_if_changed(lbl_hum[i], buf);
        } else {
            ui_label_set_text_if_changed(lbl_hum[i], "");
        }

        if (hum_mode != last_card_hum_mode[i]) {
            last_card_hum_mode[i] = hum_mode;
            if (has_hum) {
                lv_obj_align(lbl_hum[i], LV_ALIGN_TOP_RIGHT, align_x, 5);
                lv_obj_set_style_text_font(lbl_max[i], &lv_font_montserrat_10, 0);
                lv_obj_set_style_text_font(lbl_min[i], &lv_font_montserrat_10, 0);
                lv_obj_align(lbl_max[i], LV_ALIGN_TOP_RIGHT, align_x, 23);
                lv_obj_align(lbl_min[i], LV_ALIGN_TOP_RIGHT, align_x, 39);
            } else {
                lv_obj_set_style_text_font(lbl_max[i], &lv_font_montserrat_12, 0);
                lv_obj_set_style_text_font(lbl_min[i], &lv_font_montserrat_12, 0);
                lv_obj_align(lbl_max[i], LV_ALIGN_TOP_RIGHT, align_x, 10);
                lv_obj_align(lbl_min[i], LV_ALIGN_TOP_RIGHT, align_x, 34);
            }
        }

        // Active vs Toggled Off visual feedback (without container opacity layer!)
        bool is_vis = plot_track_visible[i];
        lv_color_t active_col = track_color(i);
        lv_color_t muted_col = ui_theme_muted();

        if (accents[i]) {
            ui_bg_color_if_changed(accents[i], is_vis ? active_col : ui_theme_border());
        }
        if (lbl_names[i]) {
            ui_text_color_if_changed(lbl_names[i], is_vis ? ui_theme_text() : muted_col);
        }
        ui_text_color_if_changed(lbl_vals[i], is_vis ? active_col : muted_col);
        if (cards[i]) {
            ui_border_color_if_changed(cards[i], is_vis ? ui_theme_border() : ui_theme_track());
        }
    }

    // 2. Chart History Throttling: only recalculate curves when a new minute-sample arrived,
    // window changed, plot visibility changed, or reveal animation point count changed.
    int win = constrain(state.temps_history_window, 1, 24);
    uint32_t hist_ver = temp_history_get_version();
    int points_to_show = temps_animating ? anim_revealed_points : CHART_POINTS;
    if (points_to_show < 2) points_to_show = 2;
    if (points_to_show > CHART_POINTS) points_to_show = CHART_POINTS;

    bool chart_needs_update = (win != last_rendered_win) ||
                              (hist_ver != last_rendered_hist_ver) ||
                              (points_to_show != last_rendered_points_to_show);

    if (!chart_needs_update) return;

    last_rendered_win = win;
    last_rendered_hist_ver = hist_ver;
    last_rendered_points_to_show = points_to_show;

    float min_val = 0.0f, max_val = 0.0f;
    float overall_min = 999.0f;
    float overall_max = -999.0f;
    bool has_any_series = false;

    auto update_track = [&](int track_idx, lv_chart_series_t *ser, bool active_sensor, lv_obj_t *l_max, lv_obj_t *l_min) {
        if (active_sensor && temp_history_get_series(track_idx, win, cached_points[track_idx], CHART_POINTS, &min_val, &max_val)) {
            cached_track_valid[track_idx] = true;
            if (plot_track_visible[track_idx]) {
                for (int i = 0; i < CHART_POINTS; ++i) {
                    if (i < points_to_show) {
                        lv_chart_set_value_by_id(chart, ser, i, cached_points[track_idx][i]);
                    } else {
                        lv_chart_set_value_by_id(chart, ser, i, LV_CHART_POINT_NONE);
                    }
                }
                if (min_val < overall_min) overall_min = min_val;
                if (max_val > overall_max) overall_max = max_val;
                has_any_series = true;
            } else {
                for (int i = 0; i < CHART_POINTS; ++i) {
                    lv_chart_set_value_by_id(chart, ser, i, LV_CHART_POINT_NONE);
                }
            }
            snprintf(buf, sizeof(buf), "Max: %.1f C", max_val);
            ui_label_set_text_if_changed(l_max, buf);
            snprintf(buf, sizeof(buf), "Min: %.1f C", min_val);
            ui_label_set_text_if_changed(l_min, buf);
        } else {
            cached_track_valid[track_idx] = false;
            for (int i = 0; i < CHART_POINTS; ++i) {
                lv_chart_set_value_by_id(chart, ser, i, LV_CHART_POINT_NONE);
            }
            ui_label_set_text_if_changed(l_max, "Max: --");
            ui_label_set_text_if_changed(l_min, "Min: --");
        }
    };

    update_track(0, ser_outdoor, true,      lbl_max[0], lbl_min[0]);
    update_track(1, ser_indoor,  true,      lbl_max[1], lbl_min[1]);
    update_track(2, ser_custom1, c1_active, lbl_max[2], lbl_min[2]);
    update_track(3, ser_custom2, c2_active, lbl_max[3], lbl_min[3]);

    // Auto-scale Y-axis range based on currently visible tracks
    if (has_any_series) {
        float span = overall_max - overall_min;
        if (span < 4.0f) {
            float mid = (overall_max + overall_min) * 0.5f;
            overall_min = mid - 2.0f;
            overall_max = mid + 2.0f;
        } else {
            overall_min = floorf(overall_min - 0.5f);
            overall_max = ceilf(overall_max + 0.5f);
        }
    } else {
        overall_min = 10.0f;
        overall_max = 30.0f;
    }

    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (lv_coord_t)roundf(overall_min * 10.0f), (lv_coord_t)roundf(overall_max * 10.0f));

    if (lbl_scale_top) {
        snprintf(buf, sizeof(buf), "Max: %.1f C", overall_max);
        ui_label_set_text_if_changed(lbl_scale_top, buf);
    }
    if (lbl_scale_bot) {
        snprintf(buf, sizeof(buf), "Min: %.1f C", overall_min);
        ui_label_set_text_if_changed(lbl_scale_bot, buf);
    }

    lv_chart_refresh(chart);
}
