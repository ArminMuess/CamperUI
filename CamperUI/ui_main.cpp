#include "ui_main.h"
#include "ui_victron.h"
#include "ui_radio.h"
#include "ui_powerunit.h"
#include "ui_energy_history.h"
#include "wifi_diagnostics.h"
#include "buzzer.h"
#include <WiFi.h>
#include "HWCDC.h"
#include <time.h>

#include "debug_log.h"

void ui_build_home(lv_obj_t *parent);

lv_obj_t *scr_main;
lv_obj_t *scr_settings;

static lv_obj_t *status_bar;
static lv_obj_t *lbl_time;
static lv_obj_t *lbl_wifi;
static lv_obj_t *lbl_soc_icon;
static lv_obj_t *lbl_soc;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_pump_status;
static lv_obj_t *lbl_heater_status;
static lv_obj_t *lbl_maxxfan_status;

// Smart Header Badges
static lv_obj_t *badge_cont;
static lv_obj_t *badge_frost;
static lv_obj_t *badge_fresh;
static lv_obj_t *badge_waste;

const TabMeta TAB_METAS[TAB_COUNT] = {
    {TAB_HOME, "Home", MDI_HOME},
    {TAB_DIMMERS, "Dimmer", MDI_LIGHTBULB},
    {TAB_POWER, "Power", MDI_BATTERY_CHARGING},
    {TAB_WATER, "Wasser", MDI_WATER},
    {TAB_CLIMATE, "Klima", MDI_FIRE},
    {TAB_TEMPS, "Temps", MDI_THERMOMETER},
    {TAB_MAXXFAN, "MaxxFan", MDI_FAN},
    {TAB_SWITCHES, "Schalter", MDI_TOGGLE_SWITCH},
    {TAB_LEVEL, "Level", MDI_SPIRIT_LEVEL},
    {TAB_SETTINGS, "Einstellungen", MDI_TUNE},
    {TAB_TRUMA, "Truma", MDI_FIRE},
    {TAB_VICTRON, "Victron", "VE"},
    {TAB_FRIDGE, "Kühlschrank", MDI_SNOWFLAKE},
    {TAB_RADIO, "Radio", MDI_SLIDERS},
    {TAB_POWERUNIT, "Power Unit", MDI_BATTERY}
};

static constexpr uint8_t PAGE_COUNT = TAB_COUNT;
static constexpr lv_coord_t SCREEN_WIDTH = 480;
static constexpr lv_coord_t SCREEN_HEIGHT = 480;
static constexpr lv_coord_t STATUS_HEIGHT = 40;
static constexpr lv_coord_t NAV_HEIGHT = 60;
static constexpr lv_coord_t CONTENT_HEIGHT = SCREEN_HEIGHT - STATUS_HEIGHT - NAV_HEIGHT;
static constexpr lv_coord_t NAV_BUTTON_WIDTH = 68;
static constexpr lv_coord_t NAV_BUTTON_HEIGHT = 48;
static constexpr lv_coord_t NAV_PADDING = 3;
static constexpr lv_coord_t NAV_GAP = 4;
static lv_obj_t *content;
static lv_obj_t *pages[TAB_COUNT] = {};
static uint8_t active_page = TAB_HOME;
static bool rebuild_pending = false;
static lv_obj_t *home_nav;
static lv_obj_t *home_nav_buttons[TAB_COUNT] = {};
static lv_coord_t nav_scroll_x = 0;

// Observe coordinates; never hide a failure by repositioning the strip in a timer.
void ui_debug_navigation(const char *stage, bool force) {
    if (!scr_main || !home_nav || !content || !status_bar || rebuild_pending) return;
    static uint8_t last_faults = 0;
    static lv_area_t last_nav_area = {};
    if (!lv_obj_is_valid(home_nav) || !lv_obj_is_valid(content) || !lv_obj_is_valid(status_bar)) {
        USBSerial.printf("[NAV " CAMPERUI_VERSION "] %s: invalid UI object\n", stage);
        return;
    }

    lv_area_t nav_area, content_area, status_area, screen_area;
    lv_obj_get_coords(home_nav, &nav_area);
    lv_obj_get_coords(content, &content_area);
    lv_obj_get_coords(status_bar, &status_area);
    lv_obj_get_coords(scr_main, &screen_area);
    uint8_t faults = 0;
    auto region_ok = [&screen_area](const lv_area_t &area, lv_coord_t y, lv_coord_t height) {
        return area.x1 == screen_area.x1 && area.y1 == screen_area.y1 + y &&
               lv_area_get_width(&area) == SCREEN_WIDTH && lv_area_get_height(&area) == height;
    };
    if (!region_ok(nav_area, SCREEN_HEIGHT - NAV_HEIGHT, NAV_HEIGHT)) faults |= 1;
    if (!region_ok(content_area, STATUS_HEIGHT, CONTENT_HEIGHT)) faults |= 2;
    if (!region_ok(status_area, 0, STATUS_HEIGHT)) faults |= 4;
    if (lv_obj_get_scroll_y(home_nav) != 0) faults |= 8;
    if (lv_obj_get_parent(home_nav) != scr_main || lv_obj_has_flag(home_nav, LV_OBJ_FLAG_HIDDEN)) faults |= 16;
    if (screen_area.x1 != 0 || screen_area.y1 != 0 ||
        lv_area_get_width(&screen_area) != SCREEN_WIDTH || lv_area_get_height(&screen_area) != SCREEN_HEIGHT) faults |= 32;

    const bool moved = nav_area.x1 != last_nav_area.x1 || nav_area.y1 != last_nav_area.y1 ||
                       nav_area.x2 != last_nav_area.x2 || nav_area.y2 != last_nav_area.y2;
    if (force || faults != last_faults || (faults && moved)) {
        lv_mem_monitor_t memory;
        lv_mem_monitor(&memory);
        USBSerial.printf("[NAV " CAMPERUI_VERSION "] %s page=%u faults=%u nav=(%d,%d) %dx%d scroll=(%d,%d) "
                         "content=(%d,%d) %dx%d lvgl_free=%u frag=%u%%\n",
                         stage, (unsigned)active_page, (unsigned)faults,
                         (int)nav_area.x1, (int)nav_area.y1,
                         (int)lv_area_get_width(&nav_area), (int)lv_area_get_height(&nav_area),
                         (int)lv_obj_get_scroll_x(home_nav), (int)lv_obj_get_scroll_y(home_nav),
                         (int)content_area.x1, (int)content_area.y1,
                         (int)lv_area_get_width(&content_area), (int)lv_area_get_height(&content_area),
                         (unsigned)memory.free_size, (unsigned)memory.frag_pct);
    }
    last_faults = faults;
    last_nav_area = nav_area;
}

// The screen chrome is independent of theme layouts and parent scrolling.
static void setup_fixed_region(lv_obj_t *obj, lv_coord_t y, lv_coord_t height) {
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, 0, y);
    lv_obj_set_size(obj, SCREEN_WIDTH, height);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN |
                          LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
}

static void update_home_nav() {
    uint16_t active = active_page;
    for (int i = 0; i < PAGE_COUNT; ++i) {
        if (!home_nav_buttons[i]) continue;
        if (i == active) lv_obj_add_state(home_nav_buttons[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(home_nav_buttons[i], LV_STATE_CHECKED);
    }
    // A tap changes only the selected page. Scrolling belongs to the user.
}

static void select_page(uint8_t index) {
    if (index >= TAB_COUNT || !pages[index]) return;
    for (uint8_t i = 0; i < TAB_COUNT; ++i) {
        if (!pages[i]) continue;
        if (i == index) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    active_page = index;
    if (index == TAB_POWER) ui_trigger_power_anim();
    else if (index == TAB_WATER) ui_trigger_water_anim();
    else if (index == TAB_DIMMERS) ui_trigger_dimmers_anim();
    else if (index == TAB_SWITCHES) ui_trigger_switches_anim();
    else if (index == TAB_LEVEL) ui_trigger_level_anim();
    else if (index == TAB_TEMPS) ui_trigger_temps_anim();
    update_home_nav();
    // Refresh newly selected controls immediately, rather than waiting for
    // the next periodic update after leaving them dormant in the background.
    {
        StateLockGuard lock;
        ui_update_visible_page();
    }
}

bool ui_page_available(TabId page) { return page < TAB_COUNT && pages[page] && state.tab_enabled[page]; }
void ui_update_fridge_tab() { ui_update_fridge(); }

void ui_navigate_to(TabId page) { if (ui_page_available(page)) select_page(page); }
bool ui_is_level_tab_active() {
    return scr_main != nullptr && !rebuild_pending && active_page == TAB_LEVEL && state.tab_enabled[TAB_LEVEL];
}

static lv_timer_t *nav_hold_timer = nullptr;
static uint32_t nav_hold_start = 0;
static bool nav_hold_triggered = false;

static void nav_hold_check_cb(lv_timer_t *timer) {
    (void)timer;
    // Only active when on the main screen
    if (!scr_main || lv_scr_act() != scr_main) {
        nav_hold_start = 0;
        nav_hold_triggered = false;
        return;
    }

    // Touch anywhere in the bottom navigation bar (height 60, y: 415..480)
    if (touch_is_pressed && touch_last_point.y >= (SCREEN_HEIGHT - NAV_HEIGHT - 5)) {
        if (!nav_hold_triggered) {
            if (nav_hold_start == 0) {
                nav_hold_start = millis();
            } else if (millis() - nav_hold_start >= 5000) {
                nav_hold_start = 0;
                nav_hold_triggered = true;
                ignore_touch_until_release = true;
                buzzer_beep_force(120);
                ui_open_settings();
                lv_indev_reset(nullptr, nullptr);
            }
        }
    } else {
        nav_hold_start = 0;
        if (!touch_is_pressed) {
            nav_hold_triggered = false;
        }
    }
}

static void home_nav_button_cb(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_CLICKED) {
        if (nav_hold_triggered) {
            return;
        }

        // Guard against click if user was actively dragging/scrolling a scrollable navigation bar
        if (home_nav && lv_obj_has_flag(home_nav, LV_OBJ_FLAG_SCROLLABLE)) {
            lv_indev_t *indev = lv_indev_get_act();
            if (indev && lv_indev_get_scroll_obj(indev) != nullptr) {
                return;
            }
        }

        uint8_t target_tab = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
        select_page(target_tab);
    }
}

// Helper to configure consistent padding across all tab pages
void ui_setup_tab_page(lv_obj_t *page) {
    if (!page) return;
    lv_obj_set_style_pad_left(page, 20, 0);
    lv_obj_set_style_pad_right(page, 20, 0);
    lv_obj_set_style_pad_top(page, 10, 0);
    lv_obj_set_style_pad_bottom(page, 10, 0);
    lv_obj_set_style_bg_opa(page, 0, 0);
    lv_obj_set_style_border_width(page, 0, 0);
}

// Helper to create a styled modern card
lv_obj_t* ui_create_card(lv_obj_t *parent, int w, int h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    
    // Fast flat card styling (crisp 1px border, zero software-shadow blur overhead)
    lv_obj_set_style_shadow_width(card, 0, 0);
    
    // Theme colors
    lv_obj_set_style_bg_color(card, ui_theme_card(), 0);
    lv_obj_set_style_border_width(card, ui_border_width(), 0);
    lv_obj_set_style_border_color(card, ui_theme_border(), 0);
    
    lv_obj_set_style_radius(card, ui_radius_card(), 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    return card;
}

void ui_apply_theme() {
    lv_disp_t * disp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(disp, 
                                               ui_theme_primary(), 
                                               lv_color_hex(UI_COLOR_DANGER), 
                                               state.dark_mode, 
                                               &lv_font_montserrat_14);
    lv_disp_set_theme(disp, theme);

    lv_obj_t *scr = lv_scr_act();
    lv_color_t bg_color = ui_theme_bg();
    lv_obj_set_style_bg_color(scr, bg_color, 0);
    
    // Refresh status bar
    lv_color_t bar_bg = ui_theme_card();
    lv_obj_set_style_bg_color(status_bar, bar_bg, 0);
    lv_obj_set_style_border_color(status_bar, ui_theme_border(), 0);
    lv_obj_set_style_text_color(status_bar, ui_theme_text(), 0);

    lv_obj_set_style_bg_color(scr_main, bg_color, 0);
    if (content) lv_obj_set_style_bg_color(content, bg_color, 0);
    if (home_nav) {
        lv_obj_set_style_bg_color(home_nav, ui_theme_card(), 0);
        for (int i = 0; i < PAGE_COUNT; ++i) {
            if (!home_nav_buttons[i]) continue;
            lv_obj_set_style_bg_color(home_nav_buttons[i], ui_theme_card(), 0);
            lv_obj_set_style_text_color(home_nav_buttons[i], ui_theme_muted(), 0);
            lv_obj_set_style_bg_color(home_nav_buttons[i], ui_theme_track(), LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(home_nav_buttons[i], ui_theme_primary(), LV_STATE_CHECKED);
            lv_obj_set_style_text_color(home_nav_buttons[i], lv_color_hex(0xffffff), LV_STATE_CHECKED);
        }
    }
}

// Rebuild outside an LVGL event callback, releasing the old screens first.
static void rebuild_ui(void *) {
    rebuild_pending = false;
    lv_obj_t *old_main = scr_main;
    lv_obj_t *old_settings = scr_settings;
    if (home_nav) nav_scroll_x = lv_obj_get_scroll_x(home_nav);
    lv_obj_t *temporary = lv_obj_create(nullptr);
    lv_scr_load(temporary);
    ui_energy_history_destroy();
    ui_home_settings_destroy();
    ui_nav_settings_destroy();
    ui_home_reset(); ui_maxxfan_reset();
    ui_victron_reset();
    ui_settings_reset();
    ui_temps_reset();
    ui_power_reset();
    ui_climate_reset();
    ui_truma_reset();
    ui_level_reset();
    ui_water_reset();
    ui_switches_reset();
    ui_fridge_reset();
    ui_radio_reset();
    ui_powerunit_reset();
    if (old_main) lv_obj_del(old_main);
    if (old_settings) lv_obj_del(old_settings);
    scr_main = scr_settings = nullptr;
    lbl_debug_info = nullptr;
    ui_init();
    lv_obj_del(temporary);
}

void ui_init() {
    if (scr_main) {
        if (!rebuild_pending) {
            rebuild_pending = true;
            if (lv_async_call(rebuild_ui, nullptr) != LV_RES_OK) {
                rebuild_pending = false;
                LV_LOG_WARN("Could not schedule UI rebuild");
            }
        }
        return;
    }
    content = home_nav = nullptr;
    for (uint8_t i = 0; i < PAGE_COUNT; ++i) {
        pages[i] = home_nav_buttons[i] = nullptr;
    }
    lv_disp_t * disp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(disp, 
                                               ui_theme_primary(), 
                                               lv_color_hex(UI_COLOR_DANGER), 
                                               state.dark_mode, 
                                               &lv_font_montserrat_14);
    lv_disp_set_theme(disp, theme);
    
    // Create Main Screen
    scr_main = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr_main);
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(scr_main, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_opa(scr_main, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr_main, ui_theme_bg(), 0);
    lv_obj_set_style_text_font(scr_main, &lv_font_montserrat_14, 0);

    // Initialize Settings Screen
    ui_settings_screen_init();

    // 1. Status Bar (Height = 40px)
    status_bar = lv_obj_create(scr_main);
    setup_fixed_region(status_bar, 0, STATUS_HEIGHT);
    lv_obj_set_style_bg_color(status_bar, ui_theme_card(), 0);
    lv_obj_set_style_border_color(status_bar, ui_theme_border(), 0);
    lv_obj_set_style_text_color(status_bar, ui_theme_text(), 0);
    lv_obj_set_style_border_width(status_bar, 1, 0);
    lv_obj_set_style_border_side(status_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    // Left: Time
    lv_obj_t *box_time = lv_obj_create(status_bar);
    lv_obj_set_size(box_time, 65, 36);
    lv_obj_align(box_time, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_opa(box_time, LV_OPA_0, 0);
    lv_obj_set_style_border_width(box_time, 0, 0);
    lv_obj_set_style_pad_all(box_time, 0, 0);
    lv_obj_clear_flag(box_time, LV_OBJ_FLAG_SCROLLABLE);

    lbl_time = lv_label_create(box_time);
    lv_label_set_text(lbl_time, "--:--");
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_time);

    // Smart Header Badges Container (Middle)
    badge_cont = lv_obj_create(status_bar);
    lv_obj_set_size(badge_cont, 140, 24);
    lv_obj_align(badge_cont, LV_ALIGN_LEFT_MID, 76, 0);
    lv_obj_set_style_bg_opa(badge_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(badge_cont, 0, 0);
    lv_obj_set_style_pad_all(badge_cont, 0, 0);
    lv_obj_set_flex_flow(badge_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(badge_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(badge_cont, 4, 0);
    lv_obj_clear_flag(badge_cont, LV_OBJ_FLAG_SCROLLABLE);

    auto make_badge = [](lv_obj_t *parent, const char *sym, lv_color_t bg_c, lv_color_t text_c) -> lv_obj_t* {
        lv_obj_t *b = lv_label_create(parent);
        lv_label_set_text(b, sym);
        lv_obj_set_style_text_font(b, &ui_font_mdi_18, 0);
        lv_obj_set_style_text_color(b, text_c, 0);
        lv_obj_set_style_bg_color(b, bg_c, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_hor(b, 4, 0);
        lv_obj_set_style_pad_ver(b, 2, 0);
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
        return b;
    };

    badge_frost = make_badge(badge_cont, MDI_SNOWFLAKE, lv_color_hex(0x1a2e45), lv_color_hex(UI_COLOR_PRIMARY));
    badge_fresh = make_badge(badge_cont, MDI_WATER,     lv_color_hex(0x122e4d), lv_color_hex(UI_COLOR_PRIMARY));
    badge_waste = make_badge(badge_cont, MDI_ALERT,     lv_color_hex(0x451a1a), lv_color_hex(UI_COLOR_DANGER));

    // Right side indicators (cleanly spaced, modern MDI icons)
    lbl_wifi = lv_label_create(status_bar);
    lv_label_set_text(lbl_wifi, MDI_WIFI);
    lv_obj_set_style_text_font(lbl_wifi, &ui_font_mdi_18, 0);
    lv_obj_align(lbl_wifi, LV_ALIGN_RIGHT_MID, -16, 0);
    
    lbl_soc = lv_label_create(status_bar);
    lv_label_set_text(lbl_soc, "--%");
    lv_obj_set_style_text_font(lbl_soc, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_soc, LV_ALIGN_RIGHT_MID, -40, 0);

    lbl_soc_icon = lv_label_create(status_bar);
    lv_label_set_text(lbl_soc_icon, MDI_BATTERY);
    lv_obj_set_style_text_font(lbl_soc_icon, &ui_font_mdi_18, 0);
    lv_obj_align(lbl_soc_icon, LV_ALIGN_RIGHT_MID, -78, 0);

    lbl_temp = lv_label_create(status_bar);
    lv_label_set_text(lbl_temp, "IN: -- C");
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_RIGHT_MID, -106, 0);

    lbl_heater_status = lv_label_create(status_bar);
    lv_label_set_text(lbl_heater_status, MDI_FIRE);
    lv_obj_set_style_text_font(lbl_heater_status, &ui_font_mdi_18, 0);
    lv_obj_set_style_text_color(lbl_heater_status, lv_color_hex(UI_COLOR_DANGER), 0);
    lv_obj_align(lbl_heater_status, LV_ALIGN_RIGHT_MID, -186, 0);
    lv_obj_add_flag(lbl_heater_status, LV_OBJ_FLAG_HIDDEN);

    lbl_maxxfan_status = lv_label_create(status_bar);
    lv_label_set_text(lbl_maxxfan_status, MDI_FAN);
    lv_obj_set_style_text_font(lbl_maxxfan_status, &ui_font_mdi_18, 0);
    lv_obj_set_style_text_color(lbl_maxxfan_status, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(lbl_maxxfan_status, LV_ALIGN_RIGHT_MID, -212, 0);
    lv_obj_add_flag(lbl_maxxfan_status, LV_OBJ_FLAG_HIDDEN);

    lbl_pump_status = lv_label_create(status_bar);
    lv_label_set_text(lbl_pump_status, MDI_WATER);
    lv_obj_set_style_text_font(lbl_pump_status, &ui_font_mdi_18, 0);
    lv_obj_align(lbl_pump_status, LV_ALIGN_RIGHT_MID, -238, 0);
    lv_obj_set_style_text_color(lbl_pump_status, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_add_flag(lbl_pump_status, LV_OBJ_FLAG_HIDDEN);

    // One fixed viewport. Pages cannot move or recreate the global bars.
    content = lv_obj_create(scr_main);
    setup_fixed_region(content, STATUS_HEIGHT, CONTENT_HEIGHT);
    lv_obj_set_style_bg_color(content, ui_theme_bg(), 0);
    for (uint8_t i = 0; i < TAB_COUNT; ++i) {
        if (!state.tab_enabled[i]) {
            pages[i] = nullptr;
            continue;
        }

        pages[i] = lv_obj_create(content);
        lv_obj_set_pos(pages[i], 0, 0);
        lv_obj_set_size(pages[i], SCREEN_WIDTH, CONTENT_HEIGHT);
        lv_obj_set_style_radius(pages[i], 0, 0);
        lv_obj_set_style_border_width(pages[i], 0, 0);
        lv_obj_set_style_bg_opa(pages[i], LV_OPA_TRANSP, 0);
        lv_obj_set_scroll_dir(pages[i], LV_DIR_VER);
        lv_obj_add_flag(pages[i], LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);

        if (i != TAB_HOME) {
            ui_setup_tab_page(pages[i]);
        }

        switch (i) {
            case TAB_HOME:     ui_build_home(pages[i]); break;
            case TAB_VICTRON: ui_build_victron(pages[i]); break;
            case TAB_DIMMERS:  ui_build_dimmers(pages[i]); break;
            case TAB_POWER:    ui_build_power(pages[i]); break;
            case TAB_WATER:    ui_build_water(pages[i]); break;
            case TAB_CLIMATE:  ui_build_climate(pages[i]); break;
            case TAB_TEMPS:    ui_build_temps(pages[i]); break;
            case TAB_MAXXFAN:  ui_build_maxxfan(pages[i]); break;
            case TAB_SWITCHES: ui_build_switches(pages[i]); break;
            case TAB_LEVEL:    ui_build_level(pages[i]); break;
            case TAB_SETTINGS: ui_build_settings(pages[i]); break;
            case TAB_TRUMA:    ui_build_truma(pages[i]); break;
            case TAB_FRIDGE:   ui_build_fridge(pages[i]); break;
            case TAB_RADIO:    ui_build_radio(pages[i]); break;
            case TAB_POWERUNIT: ui_build_powerunit(pages[i]); break;
            default: break;
        }
    }

    // The only navigation strip: a fixed sibling of status bar and content.
    home_nav = lv_obj_create(scr_main);
    setup_fixed_region(home_nav, SCREEN_HEIGHT - NAV_HEIGHT, NAV_HEIGHT);
    lv_obj_set_style_pad_hor(home_nav, NAV_PADDING, 0);
    lv_obj_set_style_bg_color(home_nav, ui_theme_card(), 0);
    // Explicit positions keep button geometry identical in every state/theme.
    int enabled_count = 0;
    for (int i = 0; i < TAB_COUNT; ++i) {
        uint8_t tab_id = state.tab_order[i];
        if (tab_id < TAB_COUNT && state.tab_enabled[tab_id]) enabled_count++;
    }

    int total_fixed_w = enabled_count * NAV_BUTTON_WIDTH + (enabled_count > 0 ? (enabled_count - 1) * NAV_GAP : 0);
    int available_w = SCREEN_WIDTH - 2 * NAV_PADDING;
    bool fits = (total_fixed_w <= available_w && enabled_count > 0);

    if (fits) {
        lv_obj_clear_flag(home_nav, LV_OBJ_FLAG_SCROLLABLE);
    } else {
        lv_obj_add_flag(home_nav, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(home_nav, LV_DIR_HOR);
        lv_obj_set_scrollbar_mode(home_nav, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(home_nav, LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_clear_flag(home_nav, LV_OBJ_FLAG_SCROLL_ELASTIC);
    }

    int btn_slot = 0;
    for (int i = 0; i < TAB_COUNT; ++i) {
        uint8_t tab_id = state.tab_order[i];
        if (tab_id >= TAB_COUNT) continue;
        if (!state.tab_enabled[tab_id]) continue;

        lv_obj_t *button = lv_btn_create(home_nav);
        home_nav_buttons[tab_id] = button;
        // Remove the default button grow/transition and focus-autoscroll effects.
        lv_obj_remove_style_all(button);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

        int pos_x = 0;
        if (fits) {
            int slot_w = available_w / enabled_count;
            pos_x = btn_slot * slot_w + (slot_w - NAV_BUTTON_WIDTH) / 2;
        } else {
            pos_x = btn_slot * (NAV_BUTTON_WIDTH + NAV_GAP);
        }

        lv_obj_set_pos(button, pos_x, (NAV_HEIGHT - NAV_BUTTON_HEIGHT) / 2);
        lv_obj_set_size(button, NAV_BUTTON_WIDTH, NAV_BUTTON_HEIGHT);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(button, ui_radius_button(), 0);
        lv_obj_set_style_outline_width(button, 2, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_outline_color(button, ui_theme_primary(), LV_STATE_FOCUS_KEY);
        // Extend touch hit area into the margins so touches near borders cleanly register
        lv_obj_set_ext_click_area(button, (NAV_HEIGHT - NAV_BUTTON_HEIGHT) / 2);

        lv_obj_add_event_cb(button, home_nav_button_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)tab_id);
        lv_obj_t *icon = lv_label_create(button);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_label_set_text(icon, TAB_METAS[tab_id].icon);
        lv_obj_set_style_text_font(icon, tab_id == TAB_VICTRON ? &lv_font_montserrat_24 : &ui_font_mdi_32, 0);
        lv_obj_center(icon);

        btn_slot++;
    }
    ui_apply_theme();
    lv_obj_update_layout(scr_main);
    lv_obj_scroll_to_x(home_nav, nav_scroll_x, LV_ANIM_OFF);

    // Pick active tab: if active_page is valid and enabled, use it; otherwise use first enabled tab from tab_order
    uint8_t target_tab = TAB_HOME;
    for (int i = 0; i < TAB_COUNT; ++i) {
        uint8_t id = state.tab_order[i];
        if (id < TAB_COUNT && state.tab_enabled[id] && pages[id]) {
            target_tab = id;
            break;
        }
    }
    if (active_page < TAB_COUNT && state.tab_enabled[active_page] && pages[active_page]) {
        target_tab = active_page;
    }
    select_page(target_tab);

    lv_obj_move_foreground(status_bar);
    lv_obj_move_foreground(home_nav);
    lv_scr_load(scr_main);
    ui_update_data();
    ui_update_visible_page();
    if (!nav_hold_timer) {
        nav_hold_timer = lv_timer_create(nav_hold_check_cb, 50, nullptr);
    }
    ui_debug_navigation("init", true);
}

void ui_rebuild_water(void) {
    if (pages[TAB_WATER]) {
        lv_obj_clean(pages[TAB_WATER]);
        ui_build_water(pages[TAB_WATER]);
        if (active_page == TAB_WATER) {
            ui_update_water_tab();
        }
    }
}

void ui_rebuild_power(void) {
    if (pages[TAB_POWER]) {
        lv_obj_clean(pages[TAB_POWER]);
        ui_build_power(pages[TAB_POWER]);
        if (active_page == TAB_POWER) {
            ui_update_power_tab();
        }
    }
}

void ui_rebuild_dimmers(void) {
    if (pages[TAB_DIMMERS]) {
        lv_obj_clean(pages[TAB_DIMMERS]);
        ui_build_dimmers(pages[TAB_DIMMERS]);
        if (active_page == TAB_DIMMERS) {
            ui_update_dimmers_tab();
        }
    }
}

void ui_rebuild_switches(void) {
    if (pages[TAB_SWITCHES]) {
        lv_obj_clean(pages[TAB_SWITCHES]);
        ui_build_switches(pages[TAB_SWITCHES]);
        if (active_page == TAB_SWITCHES) {
            ui_update_switches_tab();
        }
    }
}

void ui_update_visible_page() {
    if (ui_energy_history_active()) { ui_energy_history_update(); return; }
    if (lv_scr_act() != scr_main) return;
    if (active_page >= TAB_COUNT || !pages[active_page]) return;
    switch (active_page) {
        case TAB_HOME: ui_update_home(); break;
        case TAB_VICTRON: ui_update_victron(); break;
        case TAB_DIMMERS: ui_update_dimmers_tab(); break;
        case TAB_POWER: ui_update_power_tab(); break;
        case TAB_WATER: ui_update_water_tab(); break;
        case TAB_CLIMATE: ui_update_climate_tab(); break;
        case TAB_TEMPS: ui_update_temps_tab(); break;
        case TAB_SWITCHES: ui_update_switches_tab(); break;
        case TAB_LEVEL: ui_update_level_tab(); break;
        case TAB_MAXXFAN: ui_update_maxxfan(); break;
        case TAB_TRUMA: ui_update_truma_tab(); break;
        case TAB_FRIDGE: ui_update_fridge(); break;
        case TAB_RADIO: ui_update_radio(); break;
        case TAB_POWERUNIT: ui_update_powerunit(); break;
        default: break;
    }
}

void ui_update_data() {
    if (lv_scr_act() != scr_main) return;

    // Update Status Bar Time
    if (lbl_time) {
        time_t now = time(nullptr);
        struct tm timeinfo;
        bool valid_time = (now > 100000);
        if (valid_time && localtime_r(&now, &timeinfo)) {
            char time_str[16];
            snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            ui_label_set_text_if_changed(lbl_time, time_str);
        } else {
            ui_label_set_text_if_changed(lbl_time, "--:--");
        }
    }

    // Update Status Bar
    if (state.wifi_connected) {
        lv_obj_clear_flag(lbl_wifi, LV_OBJ_FLAG_HIDDEN);
        if (state.wifi_rssi > -60) ui_text_color_if_changed(lbl_wifi, lv_color_hex(UI_COLOR_SUCCESS), 0);
        else if (state.wifi_rssi > -80) ui_text_color_if_changed(lbl_wifi, lv_color_hex(UI_COLOR_WARNING), 0);
        else ui_text_color_if_changed(lbl_wifi, lv_color_hex(UI_COLOR_DANGER), 0);
    } else {
        lv_obj_add_flag(lbl_wifi, LV_OBJ_FLAG_HIDDEN);
    }

    char soc_text[16];
    snprintf(soc_text, sizeof(soc_text), "%d%%", state.bat_soc);
    ui_label_set_text_if_changed(lbl_soc, soc_text);
    if (state.battery_icon_mode) {
        lv_obj_clear_flag(lbl_soc_icon, LV_OBJ_FLAG_HIDDEN);
        ui_text_color_if_changed(lbl_soc_icon, ui_battery_soc_color(state.bat_soc), 0);
    } else {
        lv_obj_add_flag(lbl_soc_icon, LV_OBJ_FLAG_HIDDEN);
    }

    ui_label_set_float(lbl_temp, "IN: %.1f C", state.indoor_temp);
    
    if (lbl_pump_status) {
        bool pump_on = false;
        if (state.pump_relay >= 0 && state.pump_relay < 8) {
            pump_on = state.switch_state[state.pump_relay];
        } else if (state.pump_relay >= 8 && state.pump_relay < 16) {
            pump_on = state.wrelay_state[state.pump_relay - 8];
        }
        if (pump_on) {
            lv_obj_clear_flag(lbl_pump_status, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lbl_pump_status, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (lbl_maxxfan_status) {
        if (ui_maxxfan_running()) lv_obj_clear_flag(lbl_maxxfan_status, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(lbl_maxxfan_status, LV_OBJ_FLAG_HIDDEN);
    }

    if (lbl_heater_status) {
        if (state.heating_on) {
            lv_obj_clear_flag(lbl_heater_status, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lbl_heater_status, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Smart Header Badges Evaluation
    if (badge_frost) {
        if (state.outdoor_temp <= state.warn_frost_temp && state.outdoor_temp > -50.0f) {
            lv_obj_clear_flag(badge_frost, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(badge_frost, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (badge_fresh) {
        if (state.tank_level[0] > 0 && state.tank_level[0] <= state.warn_fresh_min) {
            lv_obj_clear_flag(badge_fresh, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(badge_fresh, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (badge_waste) {
        if (state.tank_level[1] >= state.warn_waste_max) {
            lv_obj_clear_flag(badge_waste, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(badge_waste, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // Update Debug Info
    if (active_page == TAB_SETTINGS && lbl_debug_info != NULL) {
        String debug_txt = state.debug_mode ? "Demo " CAMPERUI_VERSION : CAMPERUI_VERSION;
        debug_txt += "\n" + wifi_connection_summary();
        if (WiFi.status() == WL_CONNECTED) debug_txt += "\nIP: " + WiFi.localIP().toString();
        debug_txt += state.debug_mode ? "\nDaten: Dummy" : "\nDaten: Live";
        ui_label_set_text_if_changed(lbl_debug_info, debug_txt.c_str());
    }
}
