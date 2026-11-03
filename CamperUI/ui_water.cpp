#include "ui_main.h"
#include "http_handler.h"
#include "buzzer.h"

static lv_obj_t *bar_tanks[4];
static lv_obj_t *lbl_tanks_pct[4];
static lv_obj_t *lbl_tanks_liters[4];
static lv_obj_t *lbl_tanks_name[4];
static int last_tank_level[4] = {-1, -1, -1, -1};
static String last_tank_names[4];

static lv_obj_t *btn_pump = NULL;
static lv_obj_t *lbl_pump = NULL;
static lv_obj_t *btn_drain = NULL;
static lv_obj_t *lbl_drain = NULL;

static int last_pump_state = -1;
static int last_drain_state = -1;
static uint32_t water_press_start[2] = {0, 0};
static bool water_hold_triggered[2] = {false, false};
static const uint32_t WATER_LOCK_HOLD_MS = 2000;

static bool get_water_relay_state(int relay) {
    if (relay >= 0 && relay < 16) return state.switch_state[relay];
    if (relay >= 16 && relay < 24) return state.wrelay_state[relay - 16];
    return false;
}

static bool is_water_relay_locked(int relay) {
    if (relay >= 0 && relay < 16) return state.switch_locked[relay];
    if (relay >= 16 && relay < 24) return state.wrelay_locked[relay - 16];
    return false;
}

static void toggle_water_relay_lock(int relay) {
    if (relay >= 0 && relay < 16) {
        state_set_switch_locked(relay, !state.switch_locked[relay]);
        buzzer_beep(40);
    } else if (relay >= 16 && relay < 24) {
        int idx = relay - 16;
        state_set_wrelay_locked(idx, !state.wrelay_locked[idx]);
        buzzer_beep(40);
    }
}

static void toggle_water_relay(int relay) {
    if (relay >= 0 && relay < 16) {
        if (state.switch_locked[relay]) {
            return;
        }
        http_publish_switch(relay, !state.switch_state[relay]);
    } else if (relay >= 16 && relay < 24) {
        int idx = relay - 16;
        if (state.wrelay_locked[idx]) {
            return;
        }
        http_publish_wrelay(idx, !state.wrelay_state[idx]);
    }
}

static void update_water_button_visuals(bool force = false) {
    // Pump Button
    if (btn_pump && state.pump_relay >= 0 && state.pump_relay < 24) {
        bool on = get_water_relay_state(state.pump_relay);
        bool locked = is_water_relay_locked(state.pump_relay);
        if (force || (int)on != last_pump_state) {
            last_pump_state = (int)on;
            if (on) {
                lv_obj_set_style_bg_color(btn_pump, ui_theme_relay(), 0);
                lv_obj_set_style_border_color(btn_pump, ui_theme_relay(), 0);
                ui_text_color_if_changed(lbl_pump, ui_theme_btn_active_text());
            } else {
                lv_obj_set_style_bg_color(btn_pump, ui_theme_card(), 0);
                lv_obj_set_style_border_color(btn_pump, ui_theme_border(), 0);
                ui_text_color_if_changed(lbl_pump, ui_theme_btn_text());
            }
        }

        if (lbl_pump) {
            ui_label_set_text_if_changed(lbl_pump, locked ? (MDI_LOCK "  " LV_SYMBOL_TINT "  Wasserpumpe") : (LV_SYMBOL_TINT "  Wasserpumpe"));
        }
    }

    // Drain Button
    if (btn_drain && state.drain_relay >= 0 && state.drain_relay < 24) {
        bool on = get_water_relay_state(state.drain_relay);
        bool locked = is_water_relay_locked(state.drain_relay);
        if (force || (int)on != last_drain_state) {
            last_drain_state = (int)on;
            if (on) {
                lv_obj_set_style_bg_color(btn_drain, lv_color_hex(UI_COLOR_WARNING), 0);
                lv_obj_set_style_border_color(btn_drain, lv_color_hex(0xfbbf24), 0);
                ui_text_color_if_changed(lbl_drain, ui_theme_btn_active_text());
            } else {
                lv_obj_set_style_bg_color(btn_drain, ui_theme_card(), 0);
                lv_obj_set_style_border_color(btn_drain, ui_theme_border(), 0);
                ui_text_color_if_changed(lbl_drain, ui_theme_btn_text());
            }
        }

        if (lbl_drain) {
            ui_label_set_text_if_changed(lbl_drain, locked ? (MDI_LOCK "  " LV_SYMBOL_DOWN "  Abwasser") : (LV_SYMBOL_DOWN "  Abwasser"));
        }
    }
}

static void water_btn_event_cb(lv_event_t * e) {
    int slot = (int)(intptr_t)lv_event_get_user_data(e); // 0 = pump, 1 = drain
    if (slot < 0 || slot > 1) return;
    int relay = (slot == 0) ? state.pump_relay : state.drain_relay;
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        water_press_start[slot] = millis();
        water_hold_triggered[slot] = false;
    } else if (code == LV_EVENT_PRESSING) {
        if (water_press_start[slot] != 0 && !water_hold_triggered[slot]) {
            if (millis() - water_press_start[slot] >= WATER_LOCK_HOLD_MS) {
                water_hold_triggered[slot] = true;
                water_press_start[slot] = 0;
                toggle_water_relay_lock(relay);
                update_water_button_visuals(true);
            }
        }
    } else if (code == LV_EVENT_CLICKED) {
        if (water_hold_triggered[slot]) {
            water_hold_triggered[slot] = false;
            return;
        }
        toggle_water_relay(relay);
        update_water_button_visuals(true);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        water_press_start[slot] = 0;
        if (code == LV_EVENT_PRESS_LOST) water_hold_triggered[slot] = false;
    }
}

void ui_update_water_tab() {
    int num_active = constrain(state.tank_count, 1, 4);
    for (int i = 0; i < num_active; i++) {
        if (!state.tank_enabled[i]) continue;
        if (!bar_tanks[i]) continue;

        // Dynamic update of tank name from state / VanPi
        if (lbl_tanks_name[i] && state.tank_names[i] != last_tank_names[i]) {
            last_tank_names[i] = state.tank_names[i];
            String name = state.tank_names[i];
            if (name.length() == 0) name = "Tank " + String(i + 1);
            ui_label_set_text_if_changed(lbl_tanks_name[i], name.c_str());
        }
        
        if (state.tank_level[i] != last_tank_level[i]) {
            lv_bar_set_value(bar_tanks[i], state.tank_level[i], LV_ANIM_ON);
            lv_label_set_text_fmt(lbl_tanks_pct[i], "%d%%", state.tank_level[i]);
            
            if (state.tank_show_liters && lbl_tanks_liters[i]) {
                int max_l = state.tank_max[i] > 0 ? state.tank_max[i] : 100;
                int liters = (state.tank_level[i] * max_l) / 100;
                const char *unit = (state.tank_unit[i] == 1) ? "kg" : "L";
                lv_label_set_text_fmt(lbl_tanks_liters[i], "%d / %d %s", liters, max_l, unit);
            }
            
            last_tank_level[i] = state.tank_level[i];
        }
    }
    
    update_water_button_visuals();
}

void ui_trigger_water_anim() {
    for (int i = 0; i < 4; i++) {
        if (bar_tanks[i]) {
            lv_bar_set_value(bar_tanks[i], 0, LV_ANIM_OFF);
            last_tank_level[i] = -1; // Force re-animation on next update
        }
    }
}

void ui_water_reset() {
    for (int i = 0; i < 4; i++) {
        bar_tanks[i] = NULL;
        lbl_tanks_pct[i] = NULL;
        lbl_tanks_liters[i] = NULL;
        lbl_tanks_name[i] = NULL;
        last_tank_level[i] = -1;
        last_tank_names[i] = "";
    }
    btn_pump = NULL;
    lbl_pump = NULL;
    btn_drain = NULL;
    lbl_drain = NULL;
    last_pump_state = -1;
    last_drain_state = -1;
    water_press_start[0] = water_press_start[1] = 0;
    water_hold_triggered[0] = water_hold_triggered[1] = false;
}

void ui_build_water(lv_obj_t *parent) {
    ui_water_reset();
    ui_setup_tab_page(parent);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    bool has_pump = (state.pump_relay >= 0 && state.pump_relay < 24);
    bool has_drain = (state.drain_relay >= 0 && state.drain_relay < 24);
    bool has_any_btn = has_pump || has_drain;

    // Card height: 286px when action buttons exist; 356px when no buttons (full card)
    lv_coord_t card_h = has_any_btn ? 286 : 356;
    lv_obj_t *card_tanks = ui_create_card(parent, 440, card_h);
    lv_obj_align(card_tanks, LV_ALIGN_TOP_MID, 0, 0);

    lv_color_t tank_colors[] = {
        ui_theme_tank(),              // Frischwasser (Theme Tank Color)
        lv_color_hex(0x64748b),         // Slate Gray (Grauwasser)
        lv_color_hex(0x718093),         // Gray Slate (Grauwasser 2)
        lv_color_hex(0x334155)          // Deep Slate (Schwarzwasser)
    };

    int num_active = constrain(state.tank_count, 1, 4);
    // Inner card width is 440 - 24 = 416px
    int col_width = 416 / num_active;
    int bar_w = (num_active == 1) ? 96 : (num_active == 2 ? 80 : (num_active == 3 ? 62 : 48));
    int bar_h = has_any_btn ? 160 : 200;

    for (int i = 0; i < num_active; i++) {
        state.tank_enabled[i] = true;
        int col_x = i * col_width;
        int center_x = col_x + (col_width / 2);

        // 1. Tank Name
        lbl_tanks_name[i] = lv_label_create(card_tanks);
        String name = state.tank_names[i];
        if (name.length() == 0) name = "Tank " + String(i + 1);
        last_tank_names[i] = name;
        lv_label_set_text(lbl_tanks_name[i], name.c_str());
        lv_obj_set_style_text_font(lbl_tanks_name[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_tanks_name[i], ui_theme_text(), 0);
        lv_obj_set_width(lbl_tanks_name[i], col_width);
        lv_obj_set_style_text_align(lbl_tanks_name[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl_tanks_name[i], LV_ALIGN_TOP_LEFT, col_x, 8);

        // 2. Vertical Tank Bar
        bar_tanks[i] = lv_bar_create(card_tanks);
        lv_obj_set_size(bar_tanks[i], bar_w, bar_h);
        lv_obj_align(bar_tanks[i], LV_ALIGN_TOP_LEFT, center_x - (bar_w / 2), 32);
        lv_bar_set_range(bar_tanks[i], 0, 100);
        lv_bar_set_value(bar_tanks[i], 0, LV_ANIM_OFF);
        
        lv_obj_set_style_radius(bar_tanks[i], ui_radius_card(), LV_PART_MAIN);
        lv_obj_set_style_radius(bar_tanks[i], ui_radius_card(), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(bar_tanks[i], ui_theme_track(), LV_PART_MAIN);
        lv_obj_set_style_border_width(bar_tanks[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(bar_tanks[i], ui_theme_border(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar_tanks[i], tank_colors[i], LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(bar_tanks[i], lv_color_lighten(tank_colors[i], 50), LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_dir(bar_tanks[i], LV_GRAD_DIR_VER, LV_PART_INDICATOR);
        lv_obj_set_style_anim_time(bar_tanks[i], 1200, LV_PART_MAIN);

        // 3. Percentage Label
        int pct_y = 32 + bar_h + 6;
        lbl_tanks_pct[i] = lv_label_create(card_tanks);
        lv_obj_set_style_text_font(lbl_tanks_pct[i], &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_tanks_pct[i], ui_theme_text(), 0);
        lv_obj_set_width(lbl_tanks_pct[i], col_width);
        lv_obj_set_style_text_align(lbl_tanks_pct[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lbl_tanks_pct[i], "0%");
        lv_obj_align(lbl_tanks_pct[i], LV_ALIGN_TOP_LEFT, col_x, pct_y);

        // 4. Liters Label (optional, larger font 16)
        if (state.tank_show_liters) {
            int lit_y = pct_y + 24;
            lbl_tanks_liters[i] = lv_label_create(card_tanks);
            lv_obj_set_style_text_font(lbl_tanks_liters[i], &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(lbl_tanks_liters[i], ui_theme_muted(), 0);
            lv_obj_set_width(lbl_tanks_liters[i], col_width);
            lv_obj_set_style_text_align(lbl_tanks_liters[i], LV_TEXT_ALIGN_CENTER, 0);
            int max_l = state.tank_max[i] > 0 ? state.tank_max[i] : 100;
            const char *unit = (state.tank_unit[i] == 1) ? "kg" : "L";
            lv_label_set_text_fmt(lbl_tanks_liters[i], "0 / %d %s", max_l, unit);
            lv_obj_align(lbl_tanks_liters[i], LV_ALIGN_TOP_LEFT, col_x, lit_y);
        }
    }

    for (int i = num_active; i < 4; i++) {
        state.tank_enabled[i] = false;
    }

    // ==========================================
    // 2. Action Buttons
    // ==========================================
    if (has_pump && has_drain) {
        // Both buttons (214px each)
        btn_pump = lv_btn_create(parent);
        lv_obj_set_size(btn_pump, 214, 64);
        lv_obj_align(btn_pump, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_radius(btn_pump, ui_radius_button(), 0);
        lv_obj_set_style_border_width(btn_pump, 1, 0);
        lv_obj_set_style_shadow_width(btn_pump, 0, 0);

        lbl_pump = lv_label_create(btn_pump);
        lv_label_set_text_fmt(lbl_pump, "%s  Wasserpumpe", LV_SYMBOL_TINT);
        lv_obj_set_style_text_font(lbl_pump, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(lbl_pump, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl_pump);

        lv_obj_add_event_cb(btn_pump, water_btn_event_cb, LV_EVENT_ALL, (void*)(intptr_t)0);

        btn_drain = lv_btn_create(parent);
        lv_obj_set_size(btn_drain, 214, 64);
        lv_obj_align(btn_drain, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_radius(btn_drain, ui_radius_button(), 0);
        lv_obj_set_style_border_width(btn_drain, 1, 0);
        lv_obj_set_style_shadow_width(btn_drain, 0, 0);

        lbl_drain = lv_label_create(btn_drain);
        lv_label_set_text_fmt(lbl_drain, "%s  Abwasser", LV_SYMBOL_DOWN);
        lv_obj_set_style_text_font(lbl_drain, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(lbl_drain, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl_drain);

        lv_obj_add_event_cb(btn_drain, water_btn_event_cb, LV_EVENT_ALL, (void*)(intptr_t)1);

    } else if (has_pump) {
        // Only pump button (440px wide, centered)
        btn_pump = lv_btn_create(parent);
        lv_obj_set_size(btn_pump, 440, 64);
        lv_obj_align(btn_pump, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(btn_pump, ui_radius_button(), 0);
        lv_obj_set_style_border_width(btn_pump, 1, 0);
        lv_obj_set_style_shadow_width(btn_pump, 0, 0);

        lbl_pump = lv_label_create(btn_pump);
        lv_label_set_text_fmt(lbl_pump, "%s  Wasserpumpe", LV_SYMBOL_TINT);
        lv_obj_set_style_text_font(lbl_pump, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(lbl_pump, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl_pump);

        lv_obj_add_event_cb(btn_pump, water_btn_event_cb, LV_EVENT_ALL, (void*)(intptr_t)0);

    } else if (has_drain) {
        // Only drain button (440px wide, centered)
        btn_drain = lv_btn_create(parent);
        lv_obj_set_size(btn_drain, 440, 64);
        lv_obj_align(btn_drain, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(btn_drain, ui_radius_button(), 0);
        lv_obj_set_style_border_width(btn_drain, 1, 0);
        lv_obj_set_style_shadow_width(btn_drain, 0, 0);

        lbl_drain = lv_label_create(btn_drain);
        lv_label_set_text_fmt(lbl_drain, "%s  Abwasser", LV_SYMBOL_DOWN);
        lv_obj_set_style_text_font(lbl_drain, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(lbl_drain, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl_drain);

        lv_obj_add_event_cb(btn_drain, water_btn_event_cb, LV_EVENT_ALL, (void*)(intptr_t)1);
    }

    last_pump_state = -1;
    last_drain_state = -1;
    update_water_button_visuals(true);
}
