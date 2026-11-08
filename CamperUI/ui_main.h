#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <lvgl.h>
#include "system_state.h"
#include "ui_mdi_icons.h"
#include "ui_fonts.h"
#define CAMPERUI_VERSION "V1.9.1-radio-dev.5"

#if !LV_MEM_CUSTOM && LV_MEM_SIZE < (512U * 1024U)
#error "CamperUI needs config/lv_conf.h: copy it to Arduino/libraries/lv_conf.h (LV_MEM_SIZE >= 512 KiB)."
#endif

// ==========================================
// Modern Automotive / Camper Color Palette
// ==========================================
#define UI_COLOR_BG_DARK       0x14171d
#define UI_COLOR_BG_LIGHT      0xf1f5f9
#define UI_COLOR_CARD_DARK     0x1e232b
#define UI_COLOR_CARD_LIGHT    0xffffff
#define UI_COLOR_BORDER_DARK   0x2e3545
#define UI_COLOR_BORDER_LIGHT  0xe2e8f0
#define UI_COLOR_TEXT_DARK     0xf8fafc
#define UI_COLOR_TEXT_LIGHT    0x0f172a
#define UI_COLOR_MUTED_DARK    0x94a3b8
#define UI_COLOR_MUTED_LIGHT   0x64748b

#define UI_COLOR_PRIMARY       0x0ea5e9 // Sky Blue

#define UI_COLOR_SUCCESS       0x10b981 // Emerald Green
#define UI_COLOR_WARNING       0xf59e0b // Warm Amber
#define UI_COLOR_DANGER        0xef4444 // Coral Red
#define UI_COLOR_TRACK_DARK    0x12151c // Recessed slider / tank track dark
#define UI_COLOR_TRACK_LIGHT   0xdbe2ec // Recessed slider / tank track light

inline lv_color_t ui_battery_soc_color(int soc) {
    return lv_color_hex(soc <= state.warn_bat_soc ? UI_COLOR_DANGER :
        soc <= state.warn_bat_yellow_soc ? 0xffdf00 : state.theme.color_primary);
}
inline int ui_home_temperature_source(int slot) {
    return slot == 0 ? state.indoor_temp_sensor :
        slot == 1 ? state.outdoor_temp_sensor : state.temps_custom_sensor[0];
}

// Inline helpers for theme-aware colors & geometry
static inline lv_color_t ui_theme_primary() { return lv_color_hex(state.theme.color_primary); }
static inline lv_color_t ui_theme_dimmer()  { return lv_color_hex(state.theme.color_dimmer); }
static inline lv_color_t ui_theme_relay()   { return lv_color_hex(state.theme.color_relay); }
static inline lv_color_t ui_theme_tank()    { return lv_color_hex(state.theme.color_tank); }
static inline lv_color_t ui_theme_bg() { return state.dark_mode ? lv_color_hex(state.theme.color_bg) : lv_color_hex(UI_COLOR_BG_LIGHT); }
static inline lv_color_t ui_theme_card() { return state.dark_mode ? lv_color_hex(state.theme.color_card) : lv_color_hex(UI_COLOR_CARD_LIGHT); }
static inline lv_color_t ui_theme_border() { return state.dark_mode ? lv_color_hex(state.theme.color_border) : lv_color_hex(UI_COLOR_BORDER_LIGHT); }
static inline lv_color_t ui_theme_text() { return state.dark_mode ? lv_color_hex(state.theme.color_text) : lv_color_hex(UI_COLOR_TEXT_LIGHT); }
static inline lv_color_t ui_theme_muted() { return state.dark_mode ? lv_color_hex(state.theme.color_muted) : lv_color_hex(UI_COLOR_MUTED_LIGHT); }
static inline lv_color_t ui_theme_track() { return state.dark_mode ? lv_color_hex(state.theme.color_track) : lv_color_hex(UI_COLOR_TRACK_LIGHT); }
static inline lv_color_t ui_theme_btn_text() { return state.dark_mode ? lv_color_hex(state.theme.color_btn_text) : lv_color_hex(UI_COLOR_TEXT_LIGHT); }
static inline lv_color_t ui_theme_btn_active_text() { return lv_color_hex(state.theme.color_btn_text_active); }
static inline uint8_t ui_radius_card() { return state.theme.radius_card; }
static inline uint8_t ui_radius_button() { return state.theme.radius_button; }
static inline uint8_t ui_border_width() { return state.theme.border_width; }

// Avoid reallocating text and invalidating a label when its display is unchanged.
static inline void ui_label_set_text_if_changed(lv_obj_t *lbl, const char *text) {
    if (lbl && strcmp(lv_label_get_text(lbl), text) != 0) lv_label_set_text(lbl, text);
}

static inline void ui_text_color_if_changed(lv_obj_t *obj, lv_color_t color,
                                            lv_style_selector_t selector = 0) {
    if (obj && lv_obj_get_style_text_color(obj, selector & LV_PART_ANY).full != color.full)
        lv_obj_set_style_text_color(obj, color, selector);
}

static inline void ui_bg_color_if_changed(lv_obj_t *obj, lv_color_t color,
                                          lv_style_selector_t selector = 0) {
    if (obj && lv_obj_get_style_bg_color(obj, selector & LV_PART_ANY).full != color.full)
        lv_obj_set_style_bg_color(obj, color, selector);
}

static inline void ui_border_color_if_changed(lv_obj_t *obj, lv_color_t color,
                                              lv_style_selector_t selector = 0) {
    if (obj && lv_obj_get_style_border_color(obj, selector & LV_PART_ANY).full != color.full)
        lv_obj_set_style_border_color(obj, color, selector);
}

static inline void ui_line_color_if_changed(lv_obj_t *obj, lv_color_t color,
                                            lv_style_selector_t selector = 0) {
    if (obj && lv_obj_get_style_line_color(obj, selector & LV_PART_ANY).full != color.full)
        lv_obj_set_style_line_color(obj, color, selector);
}

static inline void ui_arc_color_if_changed(lv_obj_t *obj, lv_color_t color,
                                           lv_style_selector_t selector = LV_PART_INDICATOR) {
    if (obj && lv_obj_get_style_arc_color(obj, selector & LV_PART_ANY).full != color.full)
        lv_obj_set_style_arc_color(obj, color, selector);
}

static inline void ui_arc_value_if_changed(lv_obj_t *arc, int16_t val) {
    if (arc && lv_arc_get_value(arc) != val)
        lv_arc_set_value(arc, val);
}

// Robust float-to-label formatter using C runtime snprintf
static inline void ui_label_set_float(lv_obj_t *lbl, const char *fmt, float val) {
    if (!lbl) return;
    char buf[64];
    snprintf(buf, sizeof(buf), fmt, val);
    ui_label_set_text_if_changed(lbl, buf);
}

// Global UI Initialization
void ui_init();
void ui_debug_navigation(const char *stage, bool force = false);

// Hardware Touch & Display State (defined in CamperUI.ino)
extern bool display_is_on;
extern bool touch_is_pressed;
extern bool ignore_touch_until_release;
extern lv_point_t touch_last_point;

// Tab Updates
void ui_update_data();
void ui_update_home();
void ui_update_maxxfan();
void ui_home_reset();
void ui_maxxfan_reset();
void ui_settings_reset();
void ui_temps_reset();
void ui_power_reset();
void ui_climate_reset();
void ui_level_reset();
void ui_water_reset();
void ui_switches_reset();
void ui_navigate_to(TabId page);
bool ui_page_available(TabId page);
bool ui_is_level_tab_active();
void ui_home_layout_changed();
void ui_build_home_settings(lv_obj_t *parent);
void ui_home_settings_refresh();
void ui_open_home_settings();
bool ui_home_settings_is_active();
void ui_home_settings_destroy();
void ui_sync_demo_controls();
void ui_update_visible_page();
void ui_update_power_tab();
void ui_update_water_tab();
void ui_update_climate_tab();
void ui_update_truma_tab();
void ui_update_fridge_tab();
void ui_update_switches_tab();
void ui_update_dimmers_tab();
void ui_update_level_tab();
void ui_update_settings_tab();

// Anim Triggers
void ui_trigger_power_anim();
void ui_trigger_water_anim();
void ui_trigger_dimmers_anim();
void ui_trigger_switches_anim();
void ui_trigger_level_anim();
void ui_trigger_temps_anim();

// Sub-Tab Builders
void ui_build_power(lv_obj_t *parent);
void ui_build_water(lv_obj_t *parent);
void ui_rebuild_water(void);
void ui_rebuild_power(void);
void ui_rebuild_dimmers(void);
void ui_rebuild_switches(void);
void ui_build_climate(lv_obj_t *parent);
void ui_build_truma(lv_obj_t *parent);
void ui_truma_reset();
void ui_build_fridge(lv_obj_t *parent);
void ui_update_fridge(void);
void ui_fridge_reset(void);
void ui_build_switches(lv_obj_t *parent);
void ui_build_dimmers(lv_obj_t *parent);
void ui_build_level(lv_obj_t *parent);
void ui_build_settings(lv_obj_t *parent);

void ui_build_maxxfan(lv_obj_t *parent);
bool ui_maxxfan_running();

// UI Helper Functions
lv_obj_t* ui_create_card(lv_obj_t *parent, int w, int h);
void ui_apply_theme();
void ui_setup_tab_page(lv_obj_t *page);

struct TabMeta {
    uint8_t id;
    const char *name;
    const char *icon;
};

extern const TabMeta TAB_METAS[TAB_COUNT];

void ui_open_nav_settings();
void ui_build_nav_settings(lv_obj_t *parent);
bool ui_nav_settings_is_active();
void ui_nav_settings_destroy();

extern lv_obj_t *lbl_debug_info;
extern lv_obj_t *scr_main;
extern lv_obj_t *scr_settings;
void ui_settings_screen_init();
void ui_open_settings();
void ui_open_victron_settings();
void ui_build_temps(lv_obj_t *parent);
void ui_update_temps_tab();

#endif // UI_MAIN_H
