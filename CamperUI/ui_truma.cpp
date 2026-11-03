#include "ui_main.h"
#include "http_handler.h"

// ============================================================================
// Truma Combi & CP Plus Control UI
// ============================================================================

// --- Header / Diagnostic Widgets ---
static lv_obj_t *card_info = nullptr;
static lv_obj_t *lbl_model = nullptr;
static lv_obj_t *lbl_outdoor = nullptr;
static lv_obj_t *badge_230v = nullptr;
static lv_obj_t *lbl_230v = nullptr;
static lv_obj_t *dd_energy = nullptr;
static lv_obj_t *dot_online = nullptr;

// --- Thermostat Arc Widgets ---
static lv_obj_t *arc_target = nullptr;
static lv_obj_t *lbl_indoor = nullptr;
static lv_obj_t *lbl_target = nullptr;
static lv_obj_t *lbl_heater_status = nullptr;
static lv_obj_t *btn_temp_minus = nullptr;
static lv_obj_t *btn_temp_plus = nullptr;
static lv_obj_t *lbl_btn_temp_minus = nullptr;
static lv_obj_t *lbl_btn_temp_plus = nullptr;

// --- Bottom Controls: Start/Stop & Warmwasser ---
static lv_obj_t *btn_start_stop = nullptr;
static lv_obj_t *lbl_btn_start_stop = nullptr;
static lv_obj_t *cont_water = nullptr;
static lv_obj_t *btn_water_off = nullptr;
static lv_obj_t *lbl_btn_water_off = nullptr;
static lv_obj_t *btn_water_eco = nullptr;
static lv_obj_t *lbl_btn_water_eco = nullptr;
static lv_obj_t *btn_water_hot = nullptr;
static lv_obj_t *lbl_btn_water_hot = nullptr;

static int last_heating_state = -1;

// Helper: Button styling for active / inactive states
static void style_segmented_button(lv_obj_t *btn, lv_obj_t *lbl, bool active, uint32_t active_color_hex = UI_COLOR_PRIMARY) {
    if (!btn) return;
    if (active) {
        ui_bg_color_if_changed(btn, lv_color_hex(active_color_hex));
        ui_border_color_if_changed(btn, lv_color_hex(active_color_hex));
        if (lbl) ui_text_color_if_changed(lbl, ui_theme_btn_active_text());
    } else {
        ui_bg_color_if_changed(btn, ui_theme_card());
        ui_border_color_if_changed(btn, ui_theme_border());
        if (lbl) ui_text_color_if_changed(lbl, ui_theme_btn_text());
    }
}

// Update arc indicator color: Blue (5 C) -> Red (30 C)
static void update_arc_color(lv_obj_t *arc) {
    if (!arc) return;
    int val = lv_arc_get_value(arc);
    int pct = ((val - 5) * 255) / 25; // 5 C to 30 C range
    if (pct < 0) pct = 0;
    if (pct > 255) pct = 255;

    lv_color_t color = lv_color_mix(lv_color_hex(UI_COLOR_DANGER), lv_color_hex(UI_COLOR_PRIMARY), pct);

    ui_arc_color_if_changed(arc, color, LV_PART_INDICATOR);
    ui_bg_color_if_changed(arc, lv_color_hex(0xffffff), LV_PART_KNOB);
    ui_border_color_if_changed(arc, color, LV_PART_KNOB);
}

// ----------------------------------------------------------------------------
// Event Handlers
// ----------------------------------------------------------------------------

static void btn_start_stop_event_cb(lv_event_t *e) {
    (void)e;
    state.truma_hold_until = millis() + 4000;
    if (state.truma_heating_on) {
        state.truma_heating_on = false;
        http_publish_truma_air(0); // Off
    } else {
        state.truma_heating_on = true;
        if (state.truma_target_temp < 5.0f || state.truma_target_temp > 30.0f) {
            state.truma_target_temp = 22.0f;
        }
        http_publish_truma_air((int)(state.truma_target_temp + 0.5f));
    }
    ui_update_truma_tab();
}

static void arc_target_event_cb(lv_event_t *e) {
    lv_obj_t *arc = lv_event_get_target(e);
    int val = lv_arc_get_value(arc);
    state.truma_hold_until = millis() + 4000;

    state.truma_target_temp = (float)val;
    if (lbl_target) {
        lv_label_set_text_fmt(lbl_target, "Ziel: %d C", val);
    }
    update_arc_color(arc);

    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        if (state.truma_heating_on) {
            http_publish_truma_air(val);
        }
    }
}

static void btn_temp_adjust_event_cb(lv_event_t *e) {
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    state.truma_hold_until = millis() + 4000;
    float new_t = state.truma_target_temp + delta;
    if (new_t < 5.0f) new_t = 5.0f;
    if (new_t > 30.0f) new_t = 30.0f;
    state.truma_target_temp = new_t;
    int t = (int)(new_t + 0.5f);
    if (arc_target) {
        lv_arc_set_value(arc_target, t);
        update_arc_color(arc_target);
    }
    if (lbl_target) {
        lv_label_set_text_fmt(lbl_target, "Ziel: %d C", t);
    }
    if (state.truma_heating_on) {
        http_publish_truma_air(t);
    }
}

static void water_btn_event_cb(lv_event_t *e) {
    const char *mode = (const char *)lv_event_get_user_data(e);
    state.truma_hold_until = millis() + 4000;
    state.truma_water_mode = mode ? mode : "off";
    http_publish_truma_water(state.truma_water_mode.c_str());
    ui_update_truma_tab();
}

static void dd_energy_event_cb(lv_event_t *e) {
    lv_obj_t *dd = lv_event_get_target(e);
    int sel = lv_dropdown_get_selected(dd);
    state.truma_hold_until = millis() + 4000;

    const char *mode = "fuel";
    switch (sel) {
        case 0: mode = "fuel"; break;
        case 1: mode = "mix1"; break;
        case 2: mode = "mix2"; break;
        case 3: mode = "el1"; break;
        case 4: mode = "el2"; break;
        default: mode = "fuel"; break;
    }
    state.truma_energy_mode = mode;
    http_publish_truma_energy(mode);
    ui_update_truma_tab();
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------

void ui_truma_reset() {
    card_info = nullptr;
    lbl_model = nullptr;
    lbl_outdoor = nullptr;
    badge_230v = nullptr;
    lbl_230v = nullptr;
    dd_energy = nullptr;
    dot_online = nullptr;

    arc_target = nullptr;
    lbl_indoor = nullptr;
    lbl_target = nullptr;
    lbl_heater_status = nullptr;
    btn_temp_minus = nullptr;
    btn_temp_plus = nullptr;
    lbl_btn_temp_minus = nullptr;
    lbl_btn_temp_plus = nullptr;

    btn_start_stop = nullptr;
    lbl_btn_start_stop = nullptr;
    cont_water = nullptr;
    btn_water_off = nullptr;
    lbl_btn_water_off = nullptr;
    btn_water_eco = nullptr;
    lbl_btn_water_eco = nullptr;
    btn_water_hot = nullptr;
    lbl_btn_water_hot = nullptr;

    last_heating_state = -1;
}

// ----------------------------------------------------------------------------
// Build Truma Tab
// ----------------------------------------------------------------------------

void ui_build_truma(lv_obj_t *parent) {
    ui_truma_reset();
    ui_setup_tab_page(parent);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    bool has_electric = truma_has_electric(state.heater_type);

    // ========================================================================
    // 1. Top Row: Info Card (Left), 230V Badge, Energy Dropdown (Right), Online Dot
    // ========================================================================

    // Left: Model & Outdoor Temp Card
    card_info = ui_create_card(parent, has_electric ? 150 : 180, 50);
    lv_obj_align(card_info, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_all(card_info, 6, 0);

    lbl_model = lv_label_create(card_info);
    lv_label_set_text(lbl_model, state.truma_model.length() > 0 ? state.truma_model.c_str() : (has_electric ? "Truma Combi E" : "Truma Combi"));
    lv_obj_set_style_text_font(lbl_model, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_model, ui_theme_muted(), 0);
    lv_obj_align(lbl_model, LV_ALIGN_TOP_LEFT, 4, 1);

    lbl_outdoor = lv_label_create(card_info);
    lv_label_set_text(lbl_outdoor, "Außen --.- C");
    lv_obj_set_style_text_font(lbl_outdoor, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_outdoor, ui_theme_text(), 0);
    lv_obj_align(lbl_outdoor, LV_ALIGN_BOTTOM_LEFT, 4, -1);

    // Online Status Dot: Oben rechts in der Ecke als grüner oder roter Punkt
    dot_online = lv_obj_create(parent);
    lv_obj_set_size(dot_online, 14, 14);
    lv_obj_align(dot_online, LV_ALIGN_TOP_RIGHT, 0, 18);
    lv_obj_set_style_radius(dot_online, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_online, 2, 0);
    lv_obj_set_style_shadow_width(dot_online, 0, 0);
    lv_obj_clear_flag(dot_online, LV_OBJ_FLAG_SCROLLABLE);

    if (has_electric) {
        // 230V Badge (between info card and energy dropdown)
        badge_230v = ui_create_card(parent, 60, 50);
        lv_obj_align(badge_230v, LV_ALIGN_TOP_LEFT, 156, 0);
        lv_obj_set_style_pad_all(badge_230v, 0, 0);
        lv_obj_set_style_radius(badge_230v, 14, 0);

        lbl_230v = lv_label_create(badge_230v);
        lv_label_set_text(lbl_230v, "230V");
        lv_obj_set_style_text_font(lbl_230v, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(lbl_230v, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl_230v);

        // Energy Source Dropdown
        dd_energy = lv_dropdown_create(parent);
        lv_dropdown_set_options(dd_energy, "Gas / Diesel\nMix 1 (900W)\nMix 2 (1800W)\nElektro 1 (900W)\nElektro 2 (1800W)");
        lv_obj_set_size(dd_energy, 190, 50);
        lv_obj_align(dd_energy, LV_ALIGN_TOP_RIGHT, -24, 0);
        lv_obj_set_style_radius(dd_energy, 14, 0);
        lv_obj_set_style_bg_color(dd_energy, ui_theme_card(), 0);
        lv_obj_set_style_border_color(dd_energy, ui_theme_border(), 0);
        lv_obj_set_style_border_width(dd_energy, 1, 0);
        lv_obj_set_style_text_color(dd_energy, ui_theme_text(), 0);
        lv_obj_set_style_text_font(dd_energy, &lv_font_montserrat_14, 0);
        lv_obj_set_style_pad_top(dd_energy, 15, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(dd_energy, 15, LV_PART_MAIN);
        lv_obj_set_style_pad_left(dd_energy, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_right(dd_energy, 30, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(dd_energy, 0, 0);

        lv_obj_t *list = lv_dropdown_get_list(dd_energy);
        lv_obj_set_style_text_font(list, &lv_font_montserrat_16, 0);
        lv_obj_set_style_pad_all(list, 10, 0);
        lv_obj_set_style_bg_color(list, ui_theme_card(), 0);
        lv_obj_set_style_border_color(list, ui_theme_border(), 0);
        lv_obj_add_event_cb(dd_energy, dd_energy_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    // ========================================================================
    // 2. Thermostat Arc Gauge (analog zur Autoterm)
    // ========================================================================
    arc_target = lv_arc_create(parent);
    lv_obj_set_size(arc_target, 230, 230);
    lv_obj_align(arc_target, LV_ALIGN_TOP_MID, 0, 56);
    lv_arc_set_range(arc_target, 5, 30);

    lv_obj_set_style_arc_width(arc_target, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_target, ui_theme_track(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_target, 16, LV_PART_INDICATOR);

    // Tactile knob with clear distinct white background
    lv_obj_set_style_radius(arc_target, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc_target, 7, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(arc_target, 0, LV_PART_KNOB);

    lv_obj_add_event_cb(arc_target, arc_target_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(arc_target, arc_target_event_cb, LV_EVENT_RELEASED, NULL);

    // Minus Button (Left of Arc)
    btn_temp_minus = lv_btn_create(parent);
    lv_obj_set_size(btn_temp_minus, 52, 52);
    lv_obj_set_style_radius(btn_temp_minus, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_temp_minus, ui_theme_card(), 0);
    lv_obj_set_style_border_color(btn_temp_minus, ui_theme_border(), 0);
    lv_obj_set_style_border_width(btn_temp_minus, 1, 0);
    lv_obj_set_style_shadow_width(btn_temp_minus, 0, 0);
    lv_obj_align_to(btn_temp_minus, arc_target, LV_ALIGN_OUT_LEFT_MID, -18, 0);
    lv_obj_add_event_cb(btn_temp_minus, btn_temp_adjust_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);

    lbl_btn_temp_minus = lv_label_create(btn_temp_minus);
    lv_label_set_text(lbl_btn_temp_minus, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(lbl_btn_temp_minus, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_btn_temp_minus, ui_theme_btn_text(), 0);
    lv_obj_center(lbl_btn_temp_minus);

    // Plus Button (Right of Arc)
    btn_temp_plus = lv_btn_create(parent);
    lv_obj_set_size(btn_temp_plus, 52, 52);
    lv_obj_set_style_radius(btn_temp_plus, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_temp_plus, ui_theme_card(), 0);
    lv_obj_set_style_border_color(btn_temp_plus, ui_theme_border(), 0);
    lv_obj_set_style_border_width(btn_temp_plus, 1, 0);
    lv_obj_set_style_shadow_width(btn_temp_plus, 0, 0);
    lv_obj_align_to(btn_temp_plus, arc_target, LV_ALIGN_OUT_RIGHT_MID, 18, 0);
    lv_obj_add_event_cb(btn_temp_plus, btn_temp_adjust_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    lbl_btn_temp_plus = lv_label_create(btn_temp_plus);
    lv_label_set_text(lbl_btn_temp_plus, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(lbl_btn_temp_plus, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_btn_temp_plus, ui_theme_btn_text(), 0);
    lv_obj_center(lbl_btn_temp_plus);

    if (!state.heater_temp_buttons) {
        lv_obj_add_flag(btn_temp_minus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_temp_plus, LV_OBJ_FLAG_HIDDEN);
    }

    // Indoor Temp Label (Ist-Temperatur, dead-centered in arc)
    lbl_indoor = lv_label_create(parent);
    lv_obj_set_width(lbl_indoor, 200);
    lv_obj_set_style_text_align(lbl_indoor, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_indoor, "--.- C");
    lv_obj_set_style_text_font(lbl_indoor, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(lbl_indoor, ui_theme_text(), 0);
    lv_obj_align_to(lbl_indoor, arc_target, LV_ALIGN_CENTER, 0, -14);

    // Target Temp Label (Zieltemperatur in arc)
    lbl_target = lv_label_create(parent);
    lv_obj_set_width(lbl_target, 200);
    lv_obj_set_style_text_align(lbl_target, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_target, "Ziel: -- C");
    lv_obj_set_style_text_font(lbl_target, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_target, ui_theme_muted(), 0);
    lv_obj_align_to(lbl_target, arc_target, LV_ALIGN_CENTER, 0, 26);

    // Truma Heater Status Text (below the arc)
    lbl_heater_status = lv_label_create(parent);
    lv_obj_set_width(lbl_heater_status, 420);
    lv_label_set_long_mode(lbl_heater_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_heater_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_heater_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_heater_status, ui_theme_muted(), 0);
    lv_label_set_text(lbl_heater_status, "");
    lv_obj_align_to(lbl_heater_status, arc_target, LV_ALIGN_OUT_BOTTOM_MID, 0, -18);

    // ========================================================================
    // 3. Bottom Controls: Start/Stop Button (Left) & Warmwasser Selection (Right)
    // ========================================================================

    // Start/Stop Button (links)
    btn_start_stop = lv_btn_create(parent);
    lv_obj_set_size(btn_start_stop, 176, 54);
    lv_obj_align(btn_start_stop, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_radius(btn_start_stop, 14, 0);
    lv_obj_set_style_border_width(btn_start_stop, 1, 0);
    lv_obj_set_style_shadow_width(btn_start_stop, 0, 0);
    lv_obj_add_event_cb(btn_start_stop, btn_start_stop_event_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn_start_stop = lv_label_create(btn_start_stop);
    lv_label_set_text(lbl_btn_start_stop, "Start");
    lv_obj_set_style_text_font(lbl_btn_start_stop, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_btn_start_stop, ui_theme_btn_active_text(), 0);
    lv_obj_center(lbl_btn_start_stop);

    // Warmwasser Auswahl (rechts in klein)
    cont_water = lv_obj_create(parent);
    lv_obj_set_size(cont_water, 252, 54);
    lv_obj_align(cont_water, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(cont_water, 0, 0);
    lv_obj_set_style_border_width(cont_water, 0, 0);
    lv_obj_set_style_pad_all(cont_water, 0, 0);
    lv_obj_set_flex_flow(cont_water, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_water, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Button 1: Aus
    btn_water_off = lv_btn_create(cont_water);
    lv_obj_set_size(btn_water_off, 78, 54);
    lv_obj_set_style_radius(btn_water_off, 12, 0);
    lv_obj_set_style_border_width(btn_water_off, 1, 0);
    lv_obj_set_style_shadow_width(btn_water_off, 0, 0);
    lv_obj_add_event_cb(btn_water_off, water_btn_event_cb, LV_EVENT_CLICKED, (void *)"off");
    lbl_btn_water_off = lv_label_create(btn_water_off);
    lv_label_set_text(lbl_btn_water_off, "Aus");
    lv_obj_set_style_text_font(lbl_btn_water_off, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_btn_water_off);

    // Button 2: Eco (40 C)
    btn_water_eco = lv_btn_create(cont_water);
    lv_obj_set_size(btn_water_eco, 83, 54);
    lv_obj_set_style_radius(btn_water_eco, 12, 0);
    lv_obj_set_style_border_width(btn_water_eco, 1, 0);
    lv_obj_set_style_shadow_width(btn_water_eco, 0, 0);
    lv_obj_add_event_cb(btn_water_eco, water_btn_event_cb, LV_EVENT_CLICKED, (void *)"eco");
    lbl_btn_water_eco = lv_label_create(btn_water_eco);
    lv_label_set_text(lbl_btn_water_eco, "Eco\n40 C");
    lv_obj_set_style_text_font(lbl_btn_water_eco, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_btn_water_eco, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_btn_water_eco);

    // Button 3: Hot (60 C)
    btn_water_hot = lv_btn_create(cont_water);
    lv_obj_set_size(btn_water_hot, 83, 54);
    lv_obj_set_style_radius(btn_water_hot, 12, 0);
    lv_obj_set_style_border_width(btn_water_hot, 1, 0);
    lv_obj_set_style_shadow_width(btn_water_hot, 0, 0);
    lv_obj_add_event_cb(btn_water_hot, water_btn_event_cb, LV_EVENT_CLICKED, (void *)"hot");
    lbl_btn_water_hot = lv_label_create(btn_water_hot);
    lv_label_set_text(lbl_btn_water_hot, "Hot\n60 C");
    lv_obj_set_style_text_font(lbl_btn_water_hot, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_btn_water_hot, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_btn_water_hot);

    // Initial value setup
    int init_t = (int)(state.truma_target_temp + 0.5f);
    if (init_t < 5) init_t = 22;
    if (init_t > 30) init_t = 30;
    lv_arc_set_value(arc_target, init_t);
    lv_label_set_text_fmt(lbl_target, "Ziel: %d C", init_t);
    update_arc_color(arc_target);

    last_heating_state = -1;
    ui_update_truma_tab();
}

// ----------------------------------------------------------------------------
// Periodic / Live UI Update
// ----------------------------------------------------------------------------

void ui_update_truma_tab() {
    if (!arc_target) return;

    // 1. Header Updates
    if (lbl_model) {
        if (state.truma_model.length() > 0) {
            ui_label_set_text_if_changed(lbl_model, state.truma_model.c_str());
        } else {
            ui_label_set_text_if_changed(lbl_model, truma_has_electric(state.heater_type) ? "Truma Combi E" : "Truma Combi");
        }
    }

    if (lbl_outdoor) {
        if (isnan(state.outdoor_temp) || state.outdoor_temp < -40.0f) {
            ui_label_set_text_if_changed(lbl_outdoor, "Außen --.- C");
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "Außen %.1f C", state.outdoor_temp);
            ui_label_set_text_if_changed(lbl_outdoor, buf);
        }
    }

    // Online Status Dot: Oben rechts in der Ecke als grüner oder roter Punkt
    if (dot_online) {
        if (state.truma_online) {
            ui_bg_color_if_changed(dot_online, lv_color_hex(UI_COLOR_SUCCESS));
            ui_border_color_if_changed(dot_online, lv_color_hex(0x34d399));
        } else {
            ui_bg_color_if_changed(dot_online, lv_color_hex(UI_COLOR_DANGER));
            ui_border_color_if_changed(dot_online, lv_color_hex(0xf87171));
        }
    }

    // 230V Badge
    if (badge_230v && lbl_230v) {
        if (state.truma_230v) {
            ui_label_set_text_if_changed(lbl_230v, "230V");
            ui_bg_color_if_changed(badge_230v, lv_color_hex(UI_COLOR_PRIMARY));
            ui_border_color_if_changed(badge_230v, lv_color_hex(0x38bdf8));
            ui_text_color_if_changed(lbl_230v, lv_color_hex(0xffffff));
        } else {
            ui_label_set_text_if_changed(lbl_230v, "Kein\n230V");
            ui_bg_color_if_changed(badge_230v, ui_theme_card());
            ui_border_color_if_changed(badge_230v, ui_theme_border());
            ui_text_color_if_changed(lbl_230v, ui_theme_muted());
        }
    }

    // Status Label Text & Error Handling
    bool has_err = (state.truma_error.length() > 0 && 
                    state.truma_error != "none" && 
                    state.truma_error != "no" && 
                    state.truma_error != "0");

    String status_txt = "";
    lv_color_t status_col = ui_theme_muted();

    if (has_err) {
        status_txt = "Fehler: " + state.truma_error;
        status_col = lv_color_hex(UI_COLOR_DANGER);
    } else if (!state.truma_online) {
        status_txt = "Truma Offline";
        status_col = lv_color_hex(UI_COLOR_DANGER);
    } else {
        bool has_el = truma_has_electric(state.heater_type);
        String em = state.truma_energy_mode; em.toLowerCase();
        bool needs_230v = has_el && (em == "mix1" || em == "mix2" || em == "el1" || em == "el2");
        if (needs_230v && !state.truma_230v) {
            status_txt = "Kein 230V Landstrom!";
            status_col = lv_color_hex(UI_COLOR_WARNING);
        } else if (state.truma_heating_on) {
            if (state.truma_water_mode == "eco") status_txt = "Heizt | Boiler Eco (40 C)";
            else if (state.truma_water_mode == "hot") status_txt = "Heizt | Boiler Hot (60 C)";
            else status_txt = "Heizt";
            status_col = lv_color_hex(UI_COLOR_WARNING);
        } else if (state.truma_water_mode != "off" && state.truma_water_mode.length() > 0) {
            if (state.truma_water_mode == "eco") status_txt = "Warmwasser Eco (40 C)";
            else if (state.truma_water_mode == "hot") status_txt = "Warmwasser Hot (60 C)";
            else status_txt = "Warmwasser Aktiv";
            status_col = lv_color_hex(UI_COLOR_SUCCESS);
        } else {
            status_txt = "Standby";
            status_col = ui_theme_muted();
        }
    }

    if (lbl_heater_status) {
        ui_label_set_text_if_changed(lbl_heater_status, status_txt.c_str());
        ui_text_color_if_changed(lbl_heater_status, status_col);
    }

    // 2. Indoor Temp (Ist-Temperatur inside Arc)
    if (lbl_indoor) {
        if (isnan(state.indoor_temp) || state.indoor_temp < -40.0f) {
            ui_label_set_text_if_changed(lbl_indoor, "--.- C");
        } else {
            ui_label_set_float(lbl_indoor, "%.1f C", state.indoor_temp);
        }
    }

    // 3. Water Boiler Selection
    String wm = state.truma_water_mode; wm.toLowerCase();
    String hs = state.heater_status; hs.toLowerCase();
    bool w_eco = (wm == "eco" || wm == "40" || wm.indexOf("eco") >= 0 ||
                  (wm == "off" && (hs.indexOf("eco") >= 0 || hs.indexOf("40") >= 0)));
    bool w_hot = (!w_eco && (wm == "hot" || wm == "60" || wm == "boost" || wm.indexOf("hot") >= 0 ||
                  (wm == "off" && (hs.indexOf("hot") >= 0 || hs.indexOf("60") >= 0 || hs.indexOf("boost") >= 0))));
    bool w_off = (!w_eco && !w_hot);

    style_segmented_button(btn_water_off, lbl_btn_water_off, w_off, UI_COLOR_PRIMARY);
    style_segmented_button(btn_water_eco, lbl_btn_water_eco, w_eco, UI_COLOR_SUCCESS);
    style_segmented_button(btn_water_hot, lbl_btn_water_hot, w_hot, UI_COLOR_WARNING);

    // Temperature +/- buttons visibility
    if (btn_temp_minus && btn_temp_plus) {
        if (state.heater_temp_buttons) {
            lv_obj_clear_flag(btn_temp_minus, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(btn_temp_plus, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(btn_temp_minus, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_temp_plus, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 4. Energy Dropdown Selection (if active)
    if (dd_energy && !lv_dropdown_is_open(dd_energy)) {
        String em = state.truma_energy_mode; em.toLowerCase();
        int expected_sel = 0;
        if (em == "fuel" || em == "gas") expected_sel = 0;
        else if (em == "mix1") expected_sel = 1;
        else if (em == "mix2") expected_sel = 2;
        else if (em == "el1") expected_sel = 3;
        else if (em == "el2") expected_sel = 4;

        if ((int)lv_dropdown_get_selected(dd_energy) != expected_sel) {
            lv_dropdown_set_selected(dd_energy, expected_sel);
        }
    }

    // Do not touch controls while user is interacting with them
    if (lv_obj_has_state(arc_target, LV_STATE_PRESSED) || 
        (dd_energy && lv_dropdown_is_open(dd_energy))) return;

    // 5. Arc Target Temp Sync
    int expected_t = (int)(state.truma_target_temp + 0.5f);
    if (expected_t < 5) expected_t = 22;
    if (expected_t > 30) expected_t = 30;

    if (lv_arc_get_value(arc_target) != expected_t) {
        lv_arc_set_value(arc_target, expected_t);
        if (lbl_target) {
            lv_label_set_text_fmt(lbl_target, "Ziel: %d C", expected_t);
        }
        update_arc_color(arc_target);
    }

    // 6. Start/Stop Button Sync
    if ((int)state.truma_heating_on != last_heating_state) {
        last_heating_state = (int)state.truma_heating_on;
        if (btn_start_stop && lbl_btn_start_stop) {
            if (state.truma_heating_on) {
                lv_obj_set_style_bg_color(btn_start_stop, lv_color_hex(UI_COLOR_DANGER), 0);
                lv_obj_set_style_border_color(btn_start_stop, lv_color_hex(0xf87171), 0);
                ui_label_set_text_if_changed(lbl_btn_start_stop, "Stop");
            } else {
                lv_obj_set_style_bg_color(btn_start_stop, lv_color_hex(UI_COLOR_SUCCESS), 0);
                lv_obj_set_style_border_color(btn_start_stop, lv_color_hex(0x34d399), 0);
                ui_label_set_text_if_changed(lbl_btn_start_stop, "Start");
            }
        }
    }
}
