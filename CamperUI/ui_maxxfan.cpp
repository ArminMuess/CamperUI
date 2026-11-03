#include "ui_main.h"
#include "ui_maxxfan_assets.h"
#include "maxxfan_client.h"

namespace {
lv_obj_t *root, *modes[3], *adjust, *minus_btn, *plus_btn, *number_label;
lv_obj_t *lid_btn, *lid_text, *direction_btn, *direction_text, *lid_image, *info;
lv_obj_t *lbl_minus_icon = nullptr, *lbl_plus_icon = nullptr;

void show(lv_obj_t *obj, bool on) {
    if (!obj) return;
    bool is_hidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    if (on && is_hidden) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else if (!on && !is_hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
void enable(lv_obj_t *obj, bool on) {
    if (!obj) return;
    bool is_disabled = lv_obj_has_state(obj, LV_STATE_DISABLED);
    if (on && is_disabled) lv_obj_clear_state(obj, LV_STATE_DISABLED);
    else if (!on && !is_disabled) lv_obj_add_state(obj, LV_STATE_DISABLED);
}
static int last_maxxfan_mode = -1;
lv_obj_t *label(lv_obj_t *parent, const char *s, const lv_font_t *font) {
    lv_obj_t *o = lv_label_create(parent); lv_label_set_text(o, s);
    lv_obj_set_style_text_font(o, font, 0); lv_obj_set_style_text_color(o, ui_theme_text(), 0);
    return o;
}
void click(lv_event_t *e) {
    int action = (int)(uintptr_t)lv_event_get_user_data(e);
    FanStatus s = maxxfan_status();
    if (action < 3) maxxfan_request(FanAction::Mode, action);
    else if (action == 3 || action == 4) maxxfan_request(s.automatic ? FanAction::Temperature : FanAction::Speed,
        (s.automatic ? s.target : s.speed) + (action == 3 ? -1 : 1));
    else if (action == 5) maxxfan_request(FanAction::Lid, !s.lid_open);
    else if (action == 6) maxxfan_request(FanAction::Direction, !s.intake);
    ui_update_maxxfan();
}
lv_obj_t *button(lv_obj_t *parent, const char *s, int x, int y, int w, int h, int action, lv_obj_t **text = nullptr) {
    lv_obj_t *o = lv_btn_create(parent); lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, ui_radius_button(), 0);
    lv_obj_set_style_bg_color(o, ui_theme_card(), 0);
    lv_obj_set_style_border_color(o, ui_theme_border(), 0);
    lv_obj_set_style_border_width(o, ui_border_width(), 0);
    lv_obj_set_style_pad_all(o, 4, 0); lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_t *l = label(o, s, &lv_font_montserrat_18); lv_obj_center(l); if (text) *text = l;
    lv_obj_set_style_text_color(l, ui_theme_btn_text(), 0);
    lv_obj_add_event_cb(o, click, LV_EVENT_CLICKED, (void *)(uintptr_t)action); return o;
}
}
void ui_maxxfan_reset() {
    root = nullptr;
    last_maxxfan_mode = -1;
    lbl_minus_icon = lbl_plus_icon = nullptr;
}
bool ui_maxxfan_running() { FanStatus s = maxxfan_status(); return s.valid && s.power; }
void ui_update_maxxfan() {
    if (!root) return;
    FanStatus s = maxxfan_status();
    int mode = s.automatic ? 2 : s.power ? 1 : 0;
    bool ready = s.valid && !s.pending;
    for (int i = 0; i < 3; ++i) {
        bool should_be_checked = (s.valid && mode == i);
        bool is_checked = lv_obj_has_state(modes[i], LV_STATE_CHECKED);
        if (should_be_checked && !is_checked) lv_obj_add_state(modes[i], LV_STATE_CHECKED);
        else if (!should_be_checked && is_checked) lv_obj_clear_state(modes[i], LV_STATE_CHECKED);
        enable(modes[i], ready);

        lv_obj_t *l = lv_obj_get_child(modes[i], 0);
        if (should_be_checked) {
            ui_bg_color_if_changed(modes[i], ui_theme_primary(), 0);
            ui_border_color_if_changed(modes[i], ui_theme_primary(), 0);
            if (l) ui_text_color_if_changed(l, ui_theme_btn_active_text(), 0);
        } else {
            ui_bg_color_if_changed(modes[i], ui_theme_card(), 0);
            ui_border_color_if_changed(modes[i], ui_theme_border(), 0);
            if (l) ui_text_color_if_changed(l, ready ? ui_theme_btn_text() : ui_theme_muted(), 0);
        }
    }
    show(adjust, s.valid && mode != 0);
    char buf[48]; snprintf(buf, sizeof(buf), s.automatic ? "%d C" : "%d / 10", s.automatic ? s.target : s.speed);
    ui_label_set_text_if_changed(number_label, buf);
    ui_text_color_if_changed(number_label, ui_theme_text());

    int v = s.automatic ? s.target : s.speed;
    enable(minus_btn, ready && v > (s.automatic ? -2 : 1));
    enable(plus_btn, ready && v < (s.automatic ? 37 : 10));

    if (lbl_minus_icon) {
        ui_text_color_if_changed(lbl_minus_icon, lv_obj_has_state(minus_btn, LV_STATE_DISABLED) ? ui_theme_muted() : ui_theme_btn_text());
    }
    if (lbl_plus_icon) {
        ui_text_color_if_changed(lbl_plus_icon, lv_obj_has_state(plus_btn, LV_STATE_DISABLED) ? ui_theme_muted() : ui_theme_btn_text());
    }

    show(direction_btn, s.valid && mode == 1);
    show(lid_btn, !s.valid || mode != 2);
    if (mode != last_maxxfan_mode) {
        last_maxxfan_mode = mode;
        lv_obj_set_pos(lid_btn, mode == 1 ? 236 : 0, mode == 0 ? 96 : 194);
        lv_obj_set_width(lid_btn, mode == 1 ? 228 : 464);
        lv_obj_set_pos(lid_image, 162, mode == 1 ? 276 : 194);
        lv_obj_set_y(info, mode == 1 ? 338 : 292);
    }
    enable(lid_btn, ready && !s.automatic); enable(direction_btn, ready && mode == 1);
    ui_label_set_text_if_changed(lid_text, !s.valid ? "Dachhaube\n--" : s.lid_open ? "Dachhaube\nOffen" : "Dachhaube\nZu");
    ui_label_set_text_if_changed(direction_text, s.intake ? "Richtung\nRein" : "Richtung\nRaus");

    if (lid_btn) {
        if (s.lid_open) {
            ui_bg_color_if_changed(lid_btn, ui_theme_relay(), 0);
            ui_border_color_if_changed(lid_btn, ui_theme_relay(), 0);
            if (lid_text) ui_text_color_if_changed(lid_text, ui_theme_btn_active_text(), 0);
        } else {
            ui_bg_color_if_changed(lid_btn, ui_theme_card(), 0);
            ui_border_color_if_changed(lid_btn, ui_theme_border(), 0);
            if (lid_text) ui_text_color_if_changed(lid_text, ready ? ui_theme_btn_text() : ui_theme_muted(), 0);
        }
    }
    if (direction_btn) {
        ui_bg_color_if_changed(direction_btn, ui_theme_card(), 0);
        ui_border_color_if_changed(direction_btn, ui_theme_border(), 0);
        if (direction_text) ui_text_color_if_changed(direction_text, ready ? ui_theme_btn_text() : ui_theme_muted(), 0);
    }

    const lv_img_dsc_t *image = s.lid_open ? &ui_maxxfan_open : &ui_maxxfan_closed;
    if (lv_img_get_src(lid_image) != image) lv_img_set_src(lid_image, image);
    show(lid_image, s.valid);
    const char *details = s.pending ? "Warte auf Rueckmeldung..." : s.message;
    if (s.demo != state.debug_mode) details = "Datenquelle wird gewechselt...";
    else if (!s.valid && !s.demo && s.updated && millis() - s.updated > 15000) details = "Live - Daten veraltet";
    if (!s.valid && !state.debug_mode && !state.wifi_connected) details = "Live offline - WLAN nicht verbunden";
    snprintf(buf, sizeof(buf), "Stufe %d/10 | %s | Haube %s", s.speed, s.intake ? "Rein" : "Raus", s.lid_open ? "offen" : "zu");
    String status = String(details);
    if (s.valid && mode == 2) status += "\n" + String(buf);
    ui_label_set_text_if_changed(info, status.c_str());
    ui_text_color_if_changed(info, ui_theme_muted());
}
void ui_build_maxxfan(lv_obj_t *parent) {
    root = parent; lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_bg_color(parent, ui_theme_bg(), 0); lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    const char *names[] = {"Aus", "Manuell", "Auto"};
    for (int i = 0; i < 3; ++i) modes[i] = button(parent, names[i], i * 157, 0, 150, 72, i);
    adjust = lv_obj_create(parent); lv_obj_remove_style_all(adjust);
    lv_obj_set_pos(adjust, 0, 96); lv_obj_set_size(adjust, 464, 74); lv_obj_clear_flag(adjust, LV_OBJ_FLAG_SCROLLABLE);
    minus_btn = button(adjust, LV_SYMBOL_MINUS, 24, 0, 80, 72, 3, &lbl_minus_icon);
    plus_btn = button(adjust, LV_SYMBOL_PLUS, 360, 0, 80, 72, 4, &lbl_plus_icon);
    number_label = label(adjust, "--", &lv_font_montserrat_32); lv_obj_align(number_label, LV_ALIGN_CENTER, 0, 0);
    direction_btn = button(parent, "Richtung", 0, 194, 228, 76, 6, &direction_text);
    lid_btn = button(parent, "Dachhaube", 236, 194, 228, 76, 5, &lid_text);
    lid_image = lv_img_create(parent); lv_img_set_src(lid_image, &ui_maxxfan_closed);
    info = label(parent, "", &lv_font_montserrat_14); lv_obj_set_pos(info, 0, 292); lv_obj_set_width(info, 464);
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    ui_update_maxxfan();
}
