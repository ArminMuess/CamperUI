#include "ui_fridge.h"
#include "ui_main.h"
#include "http_handler.h"

// ============================================================================
// Dometic RC10 CI-Bus Control & Telemetry UI
// ============================================================================

static lv_obj_t *card_top = nullptr;
static lv_obj_t *lbl_fridge_temp = nullptr;

static lv_obj_t *btn_power = nullptr;
static lv_obj_t *lbl_power_icon = nullptr;
static lv_obj_t *lbl_power_text = nullptr;

static lv_obj_t *btn_step_minus = nullptr;
static lv_obj_t *btn_step_plus = nullptr;
static lv_obj_t *lbl_step_num = nullptr;
static lv_obj_t *step_bars[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *lbl_step_bars[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

static lv_obj_t *card_bot = nullptr;
static lv_obj_t *btn_modes[3] = {nullptr, nullptr, nullptr};
static lv_obj_t *lbl_modes[3] = {nullptr, nullptr, nullptr};

static lv_obj_t *lbl_power_source = nullptr;
static lv_obj_t *lbl_compressor = nullptr;
static lv_obj_t *lbl_fan = nullptr;
static lv_obj_t *lbl_diag_error = nullptr;

void ui_fridge_reset(void) {
    card_top = nullptr;
    lbl_fridge_temp = nullptr;
    btn_power = nullptr;
    lbl_power_icon = nullptr;
    lbl_power_text = nullptr;
    btn_step_minus = nullptr;
    btn_step_plus = nullptr;
    lbl_step_num = nullptr;
    for (int i = 0; i < 5; ++i) {
        step_bars[i] = nullptr;
        lbl_step_bars[i] = nullptr;
    }
    card_bot = nullptr;
    for (int i = 0; i < 3; ++i) {
        btn_modes[i] = nullptr;
        lbl_modes[i] = nullptr;
    }
    lbl_power_source = nullptr;
    lbl_compressor = nullptr;
    lbl_fan = nullptr;
    lbl_diag_error = nullptr;
}

// ----------------------------------------------------------------------------
// Event Handlers
// ----------------------------------------------------------------------------

static void btn_power_event_cb(lv_event_t *e) {
    (void)e;
    bool new_power = !state.fridge_power;
    const char *mode_str = (state.fridge_mode == 1) ? "silent" : ((state.fridge_mode == 2) ? "turbo" : "performance");
    http_publish_fridge(new_power, mode_str, state.fridge_step);
    ui_update_fridge();
}

static void btn_mode_event_cb(lv_event_t *e) {
    int mode = (int)(intptr_t)lv_event_get_user_data(e);
    int step = state.fridge_step;
    if (mode == 2) step = 5; // turbo always step 5
    else if (mode == 1 && step > 3) step = 3; // silent capped at 3
    const char *mode_str = (mode == 1) ? "silent" : ((mode == 2) ? "turbo" : "performance");
    http_publish_fridge(state.fridge_power, mode_str, step);
    ui_update_fridge();
}

static void btn_step_adjust_event_cb(lv_event_t *e) {
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    int cur_mode = state.fridge_mode;
    int cur_step = state.fridge_step;

    if (cur_mode == 2) {
        // In Turbo mode, adjusting down drops to Performance mode
        cur_mode = 0;
        cur_step = (delta < 0) ? 4 : 5;
    } else {
        cur_step += delta;
        int max_step = (cur_mode == 1) ? 3 : 5;
        cur_step = constrain(cur_step, 1, max_step);
    }

    const char *mode_str = (cur_mode == 1) ? "silent" : ((cur_mode == 2) ? "turbo" : "performance");
    http_publish_fridge(state.fridge_power, mode_str, cur_step);
    ui_update_fridge();
}

static void segment_event_cb(lv_event_t *e) {
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    int cur_mode = state.fridge_mode;
    if (cur_mode == 2 && step != 5) {
        cur_mode = 0; // drop turbo to performance
    } else if (cur_mode == 1 && step > 3) {
        cur_mode = 0; // selecting step 4 or 5 moves from silent to performance
    }
    const char *mode_str = (cur_mode == 1) ? "silent" : ((cur_mode == 2) ? "turbo" : "performance");
    http_publish_fridge(state.fridge_power, mode_str, step);
    ui_update_fridge();
}

// ----------------------------------------------------------------------------
// Builder
// ----------------------------------------------------------------------------

void ui_build_fridge(lv_obj_t *parent) {
    ui_fridge_reset();
    ui_setup_tab_page(parent);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // ========================================================================
    // Card 1 (Top): Power & Cooling Level (440 x 165)
    // ========================================================================
    card_top = ui_create_card(parent, 440, 165);
    lv_obj_set_pos(card_top, 0, 0);
    lv_obj_clear_flag(card_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card_top, 10, 0);

    // Refrigerator Temperature Display (Top Right, only shown if sensor is configured)
    lbl_fridge_temp = lv_label_create(card_top);
    lv_obj_align(lbl_fridge_temp, LV_ALIGN_TOP_RIGHT, -6, 2);
    lv_obj_set_style_text_align(lbl_fridge_temp, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(lbl_fridge_temp, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_fridge_temp, ui_theme_text(), 0);
    lv_label_set_text(lbl_fridge_temp, "");

    // --- Left: Power Button ---
    btn_power = lv_btn_create(card_top);
    lv_obj_set_size(btn_power, 116, 112);
    lv_obj_set_pos(btn_power, 4, 32);
    lv_obj_set_style_radius(btn_power, ui_radius_button(), 0);
    lv_obj_set_style_border_width(btn_power, ui_border_width(), 0);
    lv_obj_set_style_shadow_width(btn_power, 0, 0);
    lv_obj_add_event_cb(btn_power, btn_power_event_cb, LV_EVENT_CLICKED, nullptr);

    lbl_power_icon = lv_label_create(btn_power);
    lv_label_set_text(lbl_power_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(lbl_power_icon, &lv_font_montserrat_26, 0);
    lv_obj_align(lbl_power_icon, LV_ALIGN_CENTER, 0, -14);

    lbl_power_text = lv_label_create(btn_power);
    lv_label_set_text(lbl_power_text, "AUS");
    lv_obj_set_style_text_font(lbl_power_text, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_power_text, LV_ALIGN_CENTER, 0, 18);

    // --- Right: Cooling Level Panel ---
    lv_obj_t *cont_step = lv_obj_create(card_top);
    lv_obj_set_size(cont_step, 290, 112);
    lv_obj_set_pos(cont_step, 128, 32);
    lv_obj_set_style_bg_opa(cont_step, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_step, 0, 0);
    lv_obj_set_style_pad_all(cont_step, 0, 0);
    lv_obj_clear_flag(cont_step, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_step_title = lv_label_create(cont_step);
    lv_label_set_text(lbl_step_title, "KÜHLSTUFE");
    lv_obj_set_style_text_font(lbl_step_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_step_title, ui_theme_muted(), 0);
    lv_obj_set_pos(lbl_step_title, 4, 2);

    // Minus Button
    btn_step_minus = lv_btn_create(cont_step);
    lv_obj_set_size(btn_step_minus, 50, 48);
    lv_obj_set_pos(btn_step_minus, 4, 20);
    lv_obj_set_style_radius(btn_step_minus, ui_radius_button(), 0);
    lv_obj_set_style_bg_color(btn_step_minus, ui_theme_card(), 0);
    lv_obj_set_style_border_color(btn_step_minus, ui_theme_border(), 0);
    lv_obj_set_style_border_width(btn_step_minus, ui_border_width(), 0);
    lv_obj_set_style_shadow_width(btn_step_minus, 0, 0);
    lv_obj_add_event_cb(btn_step_minus, btn_step_adjust_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);

    lv_obj_t *lbl_minus = lv_label_create(btn_step_minus);
    lv_label_set_text(lbl_minus, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(lbl_minus, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_minus, ui_theme_btn_text(), 0);
    lv_obj_center(lbl_minus);

    // Step Level Center Display
    lbl_step_num = lv_label_create(cont_step);
    lv_label_set_text(lbl_step_num, "3 / 5");
    lv_obj_set_style_text_font(lbl_step_num, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(lbl_step_num, ui_theme_text(), 0);
    lv_obj_set_pos(lbl_step_num, 114, 28);

    // Plus Button
    btn_step_plus = lv_btn_create(cont_step);
    lv_obj_set_size(btn_step_plus, 50, 48);
    lv_obj_set_pos(btn_step_plus, 234, 20);
    lv_obj_set_style_radius(btn_step_plus, ui_radius_button(), 0);
    lv_obj_set_style_bg_color(btn_step_plus, ui_theme_card(), 0);
    lv_obj_set_style_border_color(btn_step_plus, ui_theme_border(), 0);
    lv_obj_set_style_border_width(btn_step_plus, ui_border_width(), 0);
    lv_obj_set_style_shadow_width(btn_step_plus, 0, 0);
    lv_obj_add_event_cb(btn_step_plus, btn_step_adjust_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    lv_obj_t *lbl_plus = lv_label_create(btn_step_plus);
    lv_label_set_text(lbl_plus, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(lbl_plus, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_plus, ui_theme_btn_text(), 0);
    lv_obj_center(lbl_plus);

    // 5 Interactive Step Bars
    const int bar_w = 52;
    const int bar_gap = 6;
    for (int i = 0; i < 5; ++i) {
        step_bars[i] = lv_btn_create(cont_step);
        lv_obj_set_size(step_bars[i], bar_w, 24);
        lv_obj_set_pos(step_bars[i], 4 + i * (bar_w + bar_gap), 76);
        lv_obj_set_style_radius(step_bars[i], 6, 0);
        lv_obj_set_style_border_width(step_bars[i], 1, 0);
        lv_obj_set_style_shadow_width(step_bars[i], 0, 0);
        lv_obj_add_event_cb(step_bars[i], segment_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(i + 1));

        lbl_step_bars[i] = lv_label_create(step_bars[i]);
        lv_label_set_text_fmt(lbl_step_bars[i], "%d", i + 1);
        lv_obj_set_style_text_font(lbl_step_bars[i], &lv_font_montserrat_12, 0);
        lv_obj_center(lbl_step_bars[i]);
    }

    // ========================================================================
    // Card 2 (Bottom): Mode Selection & Telemetry (440 x 180)
    // ========================================================================
    card_bot = ui_create_card(parent, 440, 180);
    lv_obj_set_pos(card_bot, 0, 177);
    lv_obj_clear_flag(card_bot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card_bot, 10, 0);

    lv_obj_t *lbl_mode_title = lv_label_create(card_bot);
    lv_label_set_text(lbl_mode_title, "BETRIEBSMODUS");
    lv_obj_set_style_text_font(lbl_mode_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_mode_title, ui_theme_muted(), 0);
    lv_obj_set_pos(lbl_mode_title, 4, 2);

    // Segmented Mode Buttons: Performance, Silent, Turbo
    const char *mode_names[3] = {"Performance", "Silent", "Turbo"};
    const int btn_mode_w = 134;
    const int btn_mode_gap = 8;
    for (int i = 0; i < 3; ++i) {
        btn_modes[i] = lv_btn_create(card_bot);
        lv_obj_set_size(btn_modes[i], btn_mode_w, 42);
        lv_obj_set_pos(btn_modes[i], 4 + i * (btn_mode_w + btn_mode_gap), 22);
        lv_obj_set_style_radius(btn_modes[i], ui_radius_button(), 0);
        lv_obj_set_style_border_width(btn_modes[i], ui_border_width(), 0);
        lv_obj_set_style_shadow_width(btn_modes[i], 0, 0);
        lv_obj_add_event_cb(btn_modes[i], btn_mode_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lbl_modes[i] = lv_label_create(btn_modes[i]);
        lv_label_set_text(lbl_modes[i], mode_names[i]);
        lv_obj_set_style_text_font(lbl_modes[i], &lv_font_montserrat_14, 0);
        lv_obj_center(lbl_modes[i]);
    }

    // Telemetry Badges Row (3 pills)
    const int pill_w = 134;
    const int pill_gap = 8;

    // 1. Stromquelle Pill
    lv_obj_t *pill_pwr = lv_obj_create(card_bot);
    lv_obj_set_size(pill_pwr, pill_w, 54);
    lv_obj_set_pos(pill_pwr, 4, 74);
    lv_obj_set_style_bg_color(pill_pwr, ui_theme_track(), 0);
    lv_obj_set_style_border_color(pill_pwr, ui_theme_border(), 0);
    lv_obj_set_style_border_width(pill_pwr, ui_border_width(), 0);
    lv_obj_set_style_radius(pill_pwr, ui_radius_button(), 0);
    lv_obj_set_style_pad_all(pill_pwr, 6, 0);
    lv_obj_clear_flag(pill_pwr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lp1 = lv_label_create(pill_pwr);
    lv_label_set_text(lp1, "Spannungsquelle");
    lv_obj_set_style_text_font(lp1, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lp1, ui_theme_muted(), 0);
    lv_obj_align(lp1, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_power_source = lv_label_create(pill_pwr);
    lv_label_set_text(lbl_power_source, "12V DC");
    lv_obj_set_style_text_font(lbl_power_source, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_power_source, ui_theme_text(), 0);
    lv_obj_align(lbl_power_source, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // 2. Kompressor Pill
    lv_obj_t *pill_comp = lv_obj_create(card_bot);
    lv_obj_set_size(pill_comp, pill_w, 54);
    lv_obj_set_pos(pill_comp, 4 + 1 * (pill_w + pill_gap), 74);
    lv_obj_set_style_bg_color(pill_comp, ui_theme_track(), 0);
    lv_obj_set_style_border_color(pill_comp, ui_theme_border(), 0);
    lv_obj_set_style_border_width(pill_comp, ui_border_width(), 0);
    lv_obj_set_style_radius(pill_comp, ui_radius_button(), 0);
    lv_obj_set_style_pad_all(pill_comp, 6, 0);
    lv_obj_clear_flag(pill_comp, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lp2 = lv_label_create(pill_comp);
    lv_label_set_text(lp2, "Kompressor");
    lv_obj_set_style_text_font(lp2, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lp2, ui_theme_muted(), 0);
    lv_obj_align(lp2, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_compressor = lv_label_create(pill_comp);
    lv_label_set_text(lbl_compressor, "Aus");
    lv_obj_set_style_text_font(lbl_compressor, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_compressor, ui_theme_text(), 0);
    lv_obj_align(lbl_compressor, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // 3. Lüfter Pill
    lv_obj_t *pill_fan = lv_obj_create(card_bot);
    lv_obj_set_size(pill_fan, pill_w, 54);
    lv_obj_set_pos(pill_fan, 4 + 2 * (pill_w + pill_gap), 74);
    lv_obj_set_style_bg_color(pill_fan, ui_theme_track(), 0);
    lv_obj_set_style_border_color(pill_fan, ui_theme_border(), 0);
    lv_obj_set_style_border_width(pill_fan, ui_border_width(), 0);
    lv_obj_set_style_radius(pill_fan, ui_radius_button(), 0);
    lv_obj_set_style_pad_all(pill_fan, 6, 0);
    lv_obj_clear_flag(pill_fan, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lp3 = lv_label_create(pill_fan);
    lv_label_set_text(lp3, "Kondensator-Lüfter");
    lv_obj_set_style_text_font(lp3, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lp3, ui_theme_muted(), 0);
    lv_obj_align(lp3, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_fan = lv_label_create(pill_fan);
    lv_label_set_text(lbl_fan, "Aus");
    lv_obj_set_style_text_font(lbl_fan, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_fan, ui_theme_text(), 0);
    lv_obj_align(lbl_fan, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Bottom Diagnostic / Status Notification
    lbl_diag_error = lv_label_create(card_bot);
    lv_label_set_text(lbl_diag_error, "Status: CI-Bus Verbunden");
    lv_obj_set_style_text_font(lbl_diag_error, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_diag_error, ui_theme_muted(), 0);
    lv_obj_set_pos(lbl_diag_error, 6, 136);

    ui_update_fridge();
}

// ----------------------------------------------------------------------------
// Periodic / Live Updater
// ----------------------------------------------------------------------------

void ui_update_fridge(void) {
    if (!card_top) return;

    bool power = false;
    uint8_t mode = 0;
    uint8_t step = 3;
    char pwr_src[8] = "12V";
    bool compressor = false;
    bool fan = false;
    char err[32] = "none";
    bool fresh = false;

    {
        StateLockGuard lock;
        power = state.fridge_power;
        mode = state.fridge_mode;
        step = constrain(state.fridge_step, 1, 5);
        if (strcasecmp(state.fridge_power_source, "AC") == 0) strncpy(pwr_src, "230V AC", sizeof(pwr_src));
        else if (strcasecmp(state.fridge_power_source, "DC24") == 0) strncpy(pwr_src, "24V DC", sizeof(pwr_src));
        else strncpy(pwr_src, "12V DC", sizeof(pwr_src));
        compressor = state.fridge_compressor;
        fan = state.fridge_fan;
        strncpy(err, state.fridge_error, sizeof(err) - 1);
        err[sizeof(err) - 1] = '\0';
        fresh = (millis() - state.fridge_last_update < 15000) || state.debug_mode;
    }

    // Refrigerator Temperature Display (Top Right)
    if (lbl_fridge_temp) {
        if (state.fridge_temp_sensor >= 0 && state.fridge_temp_sensor < TEMP_SOURCE_COUNT && state.fridge_temp > -100.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), MDI_SNOWFLAKE "  %.1f C", state.fridge_temp);
            ui_label_set_text_if_changed(lbl_fridge_temp, buf);
            ui_text_color_if_changed(lbl_fridge_temp, ui_theme_text());
            lv_obj_align(lbl_fridge_temp, LV_ALIGN_TOP_RIGHT, -6, 2);
        } else {
            ui_label_set_text_if_changed(lbl_fridge_temp, "");
        }
    }

    // Power Button Styling
    if (btn_power) {
        if (power) {
            ui_bg_color_if_changed(btn_power, ui_theme_relay());
            ui_border_color_if_changed(btn_power, ui_theme_relay());
            if (lbl_power_icon) ui_text_color_if_changed(lbl_power_icon, ui_theme_btn_active_text());
            if (lbl_power_text) {
                ui_label_set_text_if_changed(lbl_power_text, "EIN");
                ui_text_color_if_changed(lbl_power_text, ui_theme_btn_active_text());
            }
        } else {
            ui_bg_color_if_changed(btn_power, ui_theme_card());
            ui_border_color_if_changed(btn_power, ui_theme_border());
            if (lbl_power_icon) ui_text_color_if_changed(lbl_power_icon, ui_theme_muted());
            if (lbl_power_text) {
                ui_label_set_text_if_changed(lbl_power_text, "AUS");
                ui_text_color_if_changed(lbl_power_text, ui_theme_btn_text());
            }
        }
    }

    // Step Level Text
    if (lbl_step_num) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d / 5", step);
        ui_label_set_text_if_changed(lbl_step_num, buf);
        ui_text_color_if_changed(lbl_step_num, power ? ui_theme_text() : ui_theme_muted());
    }

    // Minus / Plus Buttons
    if (btn_step_minus) {
        bool can_minus = power && ((mode == 2) || (step > 1));
        ui_border_color_if_changed(btn_step_minus, can_minus ? ui_theme_border() : ui_theme_track());
    }
    if (btn_step_plus) {
        bool can_plus = power && ((mode == 0 && step < 5) || (mode == 1 && step < 3));
        ui_border_color_if_changed(btn_step_plus, can_plus ? ui_theme_border() : ui_theme_track());
    }

    // 5 Step Segment Bars
    for (int i = 0; i < 5; ++i) {
        if (!step_bars[i]) continue;
        bool active = power && ((i + 1) <= step);
        if (active) {
            ui_bg_color_if_changed(step_bars[i], ui_theme_primary());
            ui_border_color_if_changed(step_bars[i], ui_theme_primary());
            if (lbl_step_bars[i]) ui_text_color_if_changed(lbl_step_bars[i], ui_theme_btn_active_text());
        } else {
            ui_bg_color_if_changed(step_bars[i], ui_theme_track());
            ui_border_color_if_changed(step_bars[i], ui_theme_border());
            if (lbl_step_bars[i]) ui_text_color_if_changed(lbl_step_bars[i], ui_theme_muted());
        }
    }

    // 3 Mode Segmented Buttons
    for (int i = 0; i < 3; ++i) {
        if (!btn_modes[i]) continue;
        bool is_current = (mode == i);
        if (is_current) {
            ui_bg_color_if_changed(btn_modes[i], ui_theme_primary());
            ui_border_color_if_changed(btn_modes[i], ui_theme_primary());
            if (lbl_modes[i]) ui_text_color_if_changed(lbl_modes[i], ui_theme_btn_active_text());
        } else {
            ui_bg_color_if_changed(btn_modes[i], ui_theme_card());
            ui_border_color_if_changed(btn_modes[i], ui_theme_border());
            if (lbl_modes[i]) ui_text_color_if_changed(lbl_modes[i], ui_theme_btn_text());
        }
    }

    // Telemetry Pills
    if (lbl_power_source) {
        ui_label_set_text_if_changed(lbl_power_source, pwr_src);
    }
    if (lbl_compressor) {
        if (compressor) {
            ui_label_set_text_if_changed(lbl_compressor, "Aktiv");
            ui_text_color_if_changed(lbl_compressor, ui_theme_primary());
        } else {
            ui_label_set_text_if_changed(lbl_compressor, "Aus");
            ui_text_color_if_changed(lbl_compressor, ui_theme_muted());
        }
    }
    if (lbl_fan) {
        if (fan) {
            ui_label_set_text_if_changed(lbl_fan, "Aktiv");
            ui_text_color_if_changed(lbl_fan, ui_theme_dimmer());
        } else {
            ui_label_set_text_if_changed(lbl_fan, "Aus");
            ui_text_color_if_changed(lbl_fan, ui_theme_muted());
        }
    }

    // Diagnostics / Error Status
    if (lbl_diag_error) {
        if (strlen(err) > 0 && strcasecmp(err, "none") != 0 && strcasecmp(err, "no") != 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " CI-Bus: %s", err);
            ui_label_set_text_if_changed(lbl_diag_error, buf);
            ui_text_color_if_changed(lbl_diag_error, lv_color_hex(UI_COLOR_DANGER));
        } else {
            ui_label_set_text_if_changed(lbl_diag_error, fresh ? "Status: CI-Bus Normalbetrieb" : "Status: Warte auf Antwort...");
            ui_text_color_if_changed(lbl_diag_error, ui_theme_muted());
        }
    }
}

