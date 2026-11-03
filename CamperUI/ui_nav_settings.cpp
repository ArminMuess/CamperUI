#include "ui_main.h"

namespace {

static lv_obj_t *nav_settings_screen = nullptr;
static lv_obj_t *list_cont = nullptr;

static uint8_t cur_order[TAB_COUNT];
static bool cur_enabled[TAB_COUNT];

struct NavRowWidget {
    lv_obj_t *card;
    lv_obj_t *btn_up;
    lv_obj_t *btn_down;
    lv_obj_t *lbl_icon;
    lv_obj_t *lbl_name;
    lv_obj_t *sw;
};
static NavRowWidget rows[TAB_COUNT];

static void update_row_display() {
    char buf[64];
    for (int i = 0; i < TAB_COUNT; ++i) {
        uint8_t tab_id = cur_order[i];
        if (tab_id >= TAB_COUNT) tab_id = i;

        if (rows[i].lbl_icon) {
            lv_label_set_text(rows[i].lbl_icon, TAB_METAS[tab_id].icon);
            lv_obj_set_style_text_font(rows[i].lbl_icon, tab_id == TAB_VICTRON ? &lv_font_montserrat_24 : &ui_font_mdi_32, 0);
            lv_obj_set_style_text_color(rows[i].lbl_icon, cur_enabled[tab_id] ? lv_color_hex(UI_COLOR_PRIMARY) : ui_theme_muted(), 0);
        }
        if (rows[i].lbl_name) {
            const char *name = TAB_METAS[tab_id].name;
            if (tab_id == TAB_CLIMATE && is_webasto_heater(state.heater_type)) {
                name = "Webasto";
            }
            snprintf(buf, sizeof(buf), "%d. %s", i + 1, name);
            lv_label_set_text(rows[i].lbl_name, buf);
            lv_obj_set_style_text_color(rows[i].lbl_name, cur_enabled[tab_id] ? ui_theme_text() : ui_theme_muted(), 0);
        }
        if (rows[i].sw) {
            if (cur_enabled[tab_id]) {
                lv_obj_add_state(rows[i].sw, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(rows[i].sw, LV_STATE_CHECKED);
            }
            lv_obj_clear_state(rows[i].sw, LV_STATE_DISABLED);
        }
    }
}

static void up_clicked(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx > 0 && idx < TAB_COUNT) {
        uint8_t tmp = cur_order[idx];
        cur_order[idx] = cur_order[idx - 1];
        cur_order[idx - 1] = tmp;
        update_row_display();
    }
}

static void down_clicked(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < TAB_COUNT - 1) {
        uint8_t tmp = cur_order[idx];
        cur_order[idx] = cur_order[idx + 1];
        cur_order[idx + 1] = tmp;
        update_row_display();
    }
}

static void switch_toggled(lv_event_t *e) {
    int row_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (row_idx >= 0 && row_idx < TAB_COUNT) {
        uint8_t tab_id = cur_order[row_idx];
        if (tab_id >= TAB_COUNT) tab_id = row_idx;

        cur_enabled[tab_id] = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        update_row_display();
    }
}

static void build_rows(lv_obj_t *parent) {
    // Explanatory Hint at top of the list
    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, "Reihenfolge mit " LV_SYMBOL_UP " / " LV_SYMBOL_DOWN " anpassen. Schalter aktiviert/deaktiviert Tabs.\nSettings ausgeblendet? 5 Sek. lang die Tableiste gedrueckt halten oeffnet die Einstellungen!");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, ui_theme_muted(), 0);
    lv_obj_set_width(hint, 440);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);

    for (int i = 0; i < TAB_COUNT; ++i) {
        // Clean card without heavy shadows
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, 440, 56);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_bg_color(card, ui_theme_card(), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, ui_theme_border(), 0);
        lv_obj_set_style_pad_all(card, 6, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        rows[i].card = card;

        // 1. Move UP Button
        lv_obj_t *btn_up = lv_btn_create(card);
        lv_obj_set_size(btn_up, 42, 42);
        lv_obj_align(btn_up, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_radius(btn_up, 8, 0);
        lv_obj_set_style_bg_color(btn_up, ui_theme_track(), 0);
        lv_obj_set_style_border_color(btn_up, ui_theme_border(), 0);
        lv_obj_set_style_border_width(btn_up, 1, 0);
        if (i == 0) {
            lv_obj_add_state(btn_up, LV_STATE_DISABLED);
            lv_obj_set_style_bg_opa(btn_up, LV_OPA_30, LV_STATE_DISABLED);
        } else {
            lv_obj_add_event_cb(btn_up, up_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
        lv_obj_t *lbl_up = lv_label_create(btn_up);
        lv_label_set_text(lbl_up, LV_SYMBOL_UP);
        lv_obj_set_style_text_color(lbl_up, ui_theme_text(), 0);
        lv_obj_center(lbl_up);
        rows[i].btn_up = btn_up;

        // 2. Move DOWN Button
        lv_obj_t *btn_down = lv_btn_create(card);
        lv_obj_set_size(btn_down, 42, 42);
        lv_obj_align(btn_down, LV_ALIGN_LEFT_MID, 52, 0);
        lv_obj_set_style_radius(btn_down, 8, 0);
        lv_obj_set_style_bg_color(btn_down, ui_theme_track(), 0);
        lv_obj_set_style_border_color(btn_down, ui_theme_border(), 0);
        lv_obj_set_style_border_width(btn_down, 1, 0);
        if (i == TAB_COUNT - 1) {
            lv_obj_add_state(btn_down, LV_STATE_DISABLED);
            lv_obj_set_style_bg_opa(btn_down, LV_OPA_30, LV_STATE_DISABLED);
        } else {
            lv_obj_add_event_cb(btn_down, down_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
        lv_obj_t *lbl_down = lv_label_create(btn_down);
        lv_label_set_text(lbl_down, LV_SYMBOL_DOWN);
        lv_obj_set_style_text_color(lbl_down, ui_theme_text(), 0);
        lv_obj_center(lbl_down);
        rows[i].btn_down = btn_down;

        // 3. Tab Icon
        lv_obj_t *lbl_icon = lv_label_create(card);
        lv_obj_set_style_text_font(lbl_icon, &ui_font_mdi_32, 0);
        lv_obj_align(lbl_icon, LV_ALIGN_LEFT_MID, 100, 0);
        rows[i].lbl_icon = lbl_icon;

        // 4. Tab Name
        lv_obj_t *lbl_name = lv_label_create(card);
        lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_16, 0);
        lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 144, 0);
        rows[i].lbl_name = lbl_name;

        // 5. Visibility Switch
        lv_obj_t *sw = lv_switch_create(card);
        lv_obj_set_size(sw, 48, 26);
        lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_add_event_cb(sw, switch_toggled, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
        rows[i].sw = sw;
    }
}

static void save_and_restart_clicked(lv_event_t *) {
    // Commit new order and visibility to system state
    for (int i = 0; i < TAB_COUNT; ++i) {
        state.tab_order[i] = cur_order[i];
        state.tab_enabled[i] = cur_enabled[i];
    }

    // Save permanently to NVS
    state_save();

    // Show reboot message
    lv_obj_t *mbox = lv_msgbox_create(NULL, "Gespeichert", "Menue-Konfiguration gespeichert!\nCamperUI startet neu...", NULL, false);
    lv_obj_center(mbox);
    // Never re-enter LVGL or block inside a button event.
    lv_timer_t *restart_timer = lv_timer_create([](lv_timer_t *) { ESP.restart(); }, 600, nullptr);
    if (restart_timer) lv_timer_set_repeat_count(restart_timer, 1);
}

} // namespace

static void open_nav_settings_deferred(void *) {
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    if (memory.free_biggest_size < 32768) {
        Serial.printf("[NAV] Not enough LVGL memory: free=%u largest=%u\n",
                      (unsigned)memory.free_size, (unsigned)memory.free_biggest_size);
        lv_obj_t *warning = lv_msgbox_create(nullptr, "Speicher knapp",
            "Tabs koennen gerade nicht geoeffnet werden.\nLVGL-Konfiguration pruefen und neu starten.", nullptr, true);
        if (warning) lv_obj_center(warning);
        return;
    }
    // Copy current state to working buffers and validate bounds
    memcpy(cur_order, state.tab_order, sizeof(cur_order));
    memcpy(cur_enabled, state.tab_enabled, sizeof(cur_enabled));

    bool seen[TAB_COUNT] = {false};
    bool valid = true;
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (cur_order[i] >= TAB_COUNT || seen[cur_order[i]]) {
            valid = false;
            break;
        }
        seen[cur_order[i]] = true;
    }
    if (!valid) {
        for (uint8_t i = 0; i < TAB_COUNT; ++i) cur_order[i] = i;
    }

    if (!nav_settings_screen) {
        nav_settings_screen = lv_obj_create(nullptr);
        lv_obj_clear_flag(nav_settings_screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(nav_settings_screen, ui_theme_bg(), 0);
        lv_obj_set_style_text_font(nav_settings_screen, &lv_font_montserrat_14, 0);

        // 1. Top Header Bar
        lv_obj_t *header = lv_obj_create(nav_settings_screen);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_size(header, 480, 50);
        lv_obj_set_style_bg_color(header, ui_theme_card(), 0);
        lv_obj_set_style_border_color(header, ui_theme_border(), 0);
        lv_obj_set_style_border_width(header, 1, 0);
        lv_obj_set_style_radius(header, 0, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // Back Button
        lv_obj_t *back = lv_btn_create(header);
        lv_obj_set_size(back, 105, 42);
        lv_obj_align(back, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_set_style_radius(back, 8, 0);
        lv_obj_set_style_bg_color(back, ui_theme_track(), 0);
        lv_obj_set_style_border_color(back, ui_theme_border(), 0);
        lv_obj_set_style_border_width(back, 1, 0);
        lv_obj_add_event_cb(back, [](lv_event_t *) {
            lv_async_call([](void *) {
                lv_scr_load(scr_main);
                ui_nav_settings_destroy();
            }, nullptr);
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t *back_lbl = lv_label_create(back);
        lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Zurück");
        lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(back_lbl, ui_theme_text(), 0);
        lv_obj_center(back_lbl);

        // Title
        lv_obj_t *title = lv_label_create(header);
        lv_label_set_text(title, "Menüleiste anpassen");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(title, ui_theme_text(), 0);
        lv_obj_align(title, LV_ALIGN_CENTER, 20, 0);

        // 2. Middle Scrollable List Container
        list_cont = lv_obj_create(nav_settings_screen);
        lv_obj_set_pos(list_cont, 0, 52);
        lv_obj_set_size(list_cont, 480, 360);
        lv_obj_set_style_bg_opa(list_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list_cont, 0, 0);
        lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(list_cont, 16, 0);
        lv_obj_set_style_pad_ver(list_cont, 8, 0);
        lv_obj_set_style_pad_row(list_cont, 8, 0);
        lv_obj_set_scroll_dir(list_cont, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(list_cont, LV_SCROLLBAR_MODE_AUTO);

        build_rows(list_cont);

        // 3. Bottom Bar with Save & Restart Button
        lv_obj_t *footer = lv_obj_create(nav_settings_screen);
        lv_obj_set_pos(footer, 0, 416);
        lv_obj_set_size(footer, 480, 64);
        lv_obj_set_style_bg_color(footer, ui_theme_card(), 0);
        lv_obj_set_style_border_color(footer, ui_theme_border(), 0);
        lv_obj_set_style_border_width(footer, 1, 0);
        lv_obj_set_style_radius(footer, 0, 0);
        lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *btn_save = lv_btn_create(footer);
        lv_obj_set_size(btn_save, 420, 46);
        lv_obj_center(btn_save);
        lv_obj_set_style_radius(btn_save, 12, 0);
        lv_obj_set_style_bg_color(btn_save, lv_color_hex(UI_COLOR_SUCCESS), 0);
        lv_obj_set_style_shadow_width(btn_save, 0, 0);
        lv_obj_add_event_cb(btn_save, save_and_restart_clicked, LV_EVENT_CLICKED, nullptr);

        lv_obj_t *lbl_save = lv_label_create(btn_save);
        lv_label_set_text(lbl_save, LV_SYMBOL_SAVE "  Speichern & Neustarten");
        lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_save, ui_theme_btn_active_text(), 0);
        lv_obj_center(lbl_save);
    }

    update_row_display();
    lv_scr_load(nav_settings_screen);
}

void ui_open_nav_settings() {
    lv_async_call(open_nav_settings_deferred, nullptr);
}

bool ui_nav_settings_is_active() {
    return nav_settings_screen && lv_scr_act() == nav_settings_screen;
}

void ui_nav_settings_destroy() {
    if (nav_settings_screen) {
        lv_obj_del(nav_settings_screen);
        nav_settings_screen = nullptr;
        list_cont = nullptr;
        for (int i = 0; i < TAB_COUNT; ++i) {
            rows[i].card = nullptr;
            rows[i].btn_up = nullptr;
            rows[i].btn_down = nullptr;
            rows[i].lbl_icon = nullptr;
            rows[i].lbl_name = nullptr;
            rows[i].sw = nullptr;
        }
    }
}
