#include "ui_powerunit.h"
#include "powerunit_client.h"
#include "powerunit_settings.h"
#include "ui_main.h"
#include "ui_screensaver.h"
#include <math.h>
using namespace powerunit;
namespace {
lv_obj_t *root = nullptr, *status = nullptr, *soc = nullptr, *battery_line = nullptr, *remaining = nullptr, *details = nullptr;
lv_obj_t *tile_value[4] = {}, *inverter = nullptr, *inverter_label = nullptr;
lv_obj_t *toggles[3] = {}, *toggle_labels[3] = {};
lv_timer_t *refresh = nullptr;
uint32_t hint_at = 0;          // "hold to switch off" hint
constexpr Switch TOGGLE_SWITCH[3] = {Switch::Shore, Switch::Alternator, Switch::Reverse};

lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width, const lv_font_t *font) {
    auto *l = lv_label_create(parent); lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y); lv_obj_set_width(l, width);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, ui_theme_text(), 0); return l;
}
lv_obj_t *card(int x, int y, int w, int h) {
    auto *c = lv_obj_create(root); lv_obj_set_pos(c, x, y); lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, ui_theme_card(), 0); lv_obj_set_style_radius(c, ui_radius_card(), 0);
    lv_obj_set_style_border_width(c, ui_border_width(), 0); lv_obj_set_style_border_color(c, ui_theme_border(), 0);
    lv_obj_set_style_pad_all(c, 0, 0); lv_obj_set_style_shadow_width(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE); return c;
}
lv_obj_t *button(int x, int y, int w, int h) {
    auto *b = lv_btn_create(root); lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, ui_radius_button(), 0); lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_bg_color(b, ui_theme_card(), 0);
    lv_obj_set_style_border_width(b, ui_border_width(), 0); lv_obj_set_style_border_color(b, ui_theme_border(), 0);
    return b;
}
void enable(lv_obj_t *obj, bool yes) {
    if (yes) lv_obj_clear_state(obj, LV_STATE_DISABLED); else lv_obj_add_state(obj, LV_STATE_DISABLED);
}
// Inverter: tap switches on; switching off needs a long press (fridge, laptop ...).
void inverter_event(lv_event_t *e) {
    auto s = powerunit_snapshot();
    bool on = s.state.switches[uint8_t(Switch::Inverter)];
    if (!s.fresh || s.pending[uint8_t(Switch::Inverter)].active) return;
    auto code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        if (!on) powerunit_request(Switch::Inverter, true); else hint_at = lv_tick_get() | 1;
    } else if (code == LV_EVENT_LONG_PRESSED && on) {
        hint_at = 0; powerunit_request(Switch::Inverter, false);
    }
    ui_update_powerunit();
}
void toggle_event(lv_event_t *e) {
    auto which = TOGGLE_SWITCH[intptr_t(lv_event_get_user_data(e))];
    auto s = powerunit_snapshot();
    if (s.fresh) powerunit_request(which, !s.state.switches[uint8_t(which)]);
    ui_update_powerunit();
}
void format_volt(char *out, size_t len, float v) {
    if (!isfinite(v)) { snprintf(out, len, "--"); return; }
    int tenths = int(lroundf(v * 10));
    snprintf(out, len, "%d,%d V", tenths / 10, abs(tenths) % 10);
}
}  // namespace

void ui_powerunit_reset() {
    if (refresh) { lv_timer_del(refresh); refresh = nullptr; }
    ui_powerunit_settings_reset();
    root = status = soc = battery_line = remaining = details = inverter = inverter_label = nullptr;
    for (auto &t : tile_value) t = nullptr;
    for (int i = 0; i < 3; ++i) toggles[i] = toggle_labels[i] = nullptr;
    hint_at = 0;
    powerunit_set_wanted(false);
}
void ui_build_powerunit(lv_obj_t *parent) {
    root = parent; ui_setup_tab_page(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    label(root, "Power Unit", 0, 0, 384, &lv_font_montserrat_20);
    auto *gear = button(396, 0, 44, 28);
    auto *icon = lv_label_create(gear); lv_label_set_text(icon, MDI_COG);
    lv_obj_set_style_text_font(icon, &ui_font_mdi_24, 0); lv_obj_center(icon);
    lv_obj_add_event_cb(gear, [](lv_event_t *) { ui_powerunit_open_settings(); }, LV_EVENT_CLICKED, nullptr);
    status = label(root, "--", 0, 29, 440, &lv_font_montserrat_12);

    auto *bat = card(0, 50, 440, 94);
    soc = label(bat, "-- %", 14, 22, 150, &lv_font_montserrat_42);
    battery_line = label(bat, "--", 170, 10, 262, &lv_font_montserrat_20);
    remaining = label(bat, "--", 170, 40, 262, &lv_font_montserrat_14);
    details = label(bat, "--", 170, 64, 262, &lv_font_montserrat_12);
    lv_obj_set_style_text_color(details, ui_theme_muted(), 0);

    const char *captions[4] = {"Solar", "Landstrom", "Lichtmaschine", "230 V Ausgang"};
    for (int i = 0; i < 4; ++i) {
        auto *t = card(i * 112, 152, 104, 62);
        auto *c = label(t, captions[i], 8, 8, 90, &lv_font_montserrat_12);
        lv_obj_set_style_text_color(c, ui_theme_muted(), 0);
        tile_value[i] = label(t, "--", 8, 30, 90, &lv_font_montserrat_18);
    }

    inverter = button(0, 222, 440, 58);
    inverter_label = lv_label_create(inverter); lv_label_set_text(inverter_label, "Wechselrichter");
    lv_obj_set_style_text_font(inverter_label, &lv_font_montserrat_18, 0); lv_obj_center(inverter_label);
    lv_obj_add_event_cb(inverter, inverter_event, LV_EVENT_SHORT_CLICKED, nullptr);
    lv_obj_add_event_cb(inverter, inverter_event, LV_EVENT_LONG_PRESSED, nullptr);

    for (int i = 0; i < 3; ++i) {
        toggles[i] = button(i * 149, 290, 142, 50);
        toggle_labels[i] = lv_label_create(toggles[i]);
        lv_obj_set_style_text_font(toggle_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(toggle_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(toggle_labels[i], "--"); lv_obj_center(toggle_labels[i]);
        lv_obj_add_event_cb(toggles[i], toggle_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(intptr_t(i)));
    }
    refresh = lv_timer_create([](lv_timer_t *) {
        bool display = display_is_on && !ui_screensaver_is_active();
        // Connected to the broker only while the display is on: nothing runs while it is dark.
        powerunit_set_wanted(display);
        bool visible = display && root && lv_scr_act() == scr_main && !lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN);
        if (visible) ui_update_powerunit();
    }, 300, nullptr);
    ui_update_powerunit();
}
void ui_update_powerunit() {
    if (!root) return;
    auto s = powerunit_snapshot();
    const auto &st = s.state;
    char line[96];
    // Status line: command feedback, hint, alert, connection
    const char *text = s.message;
    lv_color_t color = ui_theme_text();
    bool feedback = *s.command_message && uint32_t(millis() - s.command_message_at) < 8000;
    if (hint_at && lv_tick_elaps(hint_at) < 3000) text = "Zum Ausschalten den Wechselrichter-Knopf gedrückt halten";
    else if (feedback) { text = s.command_message; color = lv_color_hex(UI_COLOR_WARNING); }
    else if (s.fresh && st.alert_count) {
        if (st.alert_count > 1) snprintf(line, sizeof(line), "%s (+%u)", st.alert, unsigned(st.alert_count - 1));
        else snprintf(line, sizeof(line), "%s", st.alert);
        text = line; color = lv_color_hex(UI_COLOR_DANGER);
    } else if (s.fresh) text = "Live-Daten der Power Unit";
    ui_label_set_text_if_changed(status, text);
    ui_text_color_if_changed(status, color);

    if (s.fresh) {
        snprintf(line, sizeof(line), "%d %%", int(lroundf(st.soc)));
        ui_label_set_text_if_changed(soc, line);
        ui_text_color_if_changed(soc, ui_battery_soc_color(int(lroundf(st.soc))));
        char p[24]; format_power(p, sizeof(p), isfinite(st.battery_power) ? fabsf(st.battery_power) : NAN);
        bool charging = isfinite(st.battery_power) && st.battery_power > 1;
        bool using_power = isfinite(st.battery_power) && st.battery_power < -1;
        snprintf(line, sizeof(line), "%s %s", p, charging ? "lädt" : using_power ? "Verbrauch" : "");
        ui_label_set_text_if_changed(battery_line, isfinite(st.battery_power) ? line : "Akku");
        char d[24]; format_duration(d, sizeof(d), st.remaining_minutes);
        snprintf(line, sizeof(line), st.remaining_minutes <= 0 ? "Restzeit: --" :
                 !strcmp(st.remaining_mode, "charge") ? "Voll in %s" : "Reicht noch %s", d);
        ui_label_set_text_if_changed(remaining, line);
        char v[16], sv[16]; format_volt(v, sizeof(v), st.battery_voltage); format_volt(sv, sizeof(sv), st.starter_voltage);
        if (isfinite(st.battery_temperature))
            snprintf(line, sizeof(line), "%s | %d C | Starter %s", v, int(lroundf(st.battery_temperature)), sv);
        else snprintf(line, sizeof(line), "%s | Starter %s", v, sv);
        ui_label_set_text_if_changed(details, line);
        float values[4] = {st.pv_power, st.shore_power, st.alternator_power, st.ac_power};
        for (int i = 0; i < 4; ++i) {
            char w[24]; format_power(w, sizeof(w), values[i]);
            if (i == 2 && !strcmp(st.alternator_mode, "reverse")) snprintf(line, sizeof(line), "%s ->", w);
            else snprintf(line, sizeof(line), "%s", w);
            ui_label_set_text_if_changed(tile_value[i], line);
        }
    } else {
        ui_label_set_text_if_changed(soc, "-- %");
        ui_text_color_if_changed(soc, ui_theme_muted());
        ui_label_set_text_if_changed(battery_line, "Keine Daten");
        ui_label_set_text_if_changed(remaining, "--");
        ui_label_set_text_if_changed(details, "--");
        for (auto *t : tile_value) ui_label_set_text_if_changed(t, "--");
    }

    // Inverter button
    const auto &inv = s.pending[uint8_t(Switch::Inverter)];
    bool on = st.switches[uint8_t(Switch::Inverter)];
    if (inv.active) snprintf(line, sizeof(line), "Wechselrichter wird %s ...", inv.target ? "eingeschaltet" : "ausgeschaltet");
    else if (!s.fresh) snprintf(line, sizeof(line), "Wechselrichter: --");
    else snprintf(line, sizeof(line), on ? "Wechselrichter: EIN" : "Wechselrichter: AUS  (tippen = ein)");
    ui_label_set_text_if_changed(inverter_label, line);
    lv_color_t bg = inv.active ? lv_color_hex(UI_COLOR_WARNING) : (s.fresh && on) ? ui_theme_relay() : ui_theme_card();
    ui_bg_color_if_changed(inverter, bg);
    ui_text_color_if_changed(inverter_label, inv.active || (s.fresh && on) ? ui_theme_btn_active_text() : ui_theme_btn_text());
    enable(inverter, s.fresh && !inv.active);

    // Charge power / reverse charging toggles
    const char *names[3] = {"Landstrom laden", "Lichtmaschine laden", "Starter laden"};
    for (int i = 0; i < 3; ++i) {
        auto which = TOGGLE_SWITCH[i];
        const auto &p = s.pending[uint8_t(which)];
        bool value = p.active ? p.target : st.switches[uint8_t(which)];
        const char *state_text = !s.fresh ? "--" : p.active ? "..." :
            which == Switch::Reverse ? (value ? "Erzwungen" : "Automatisch") : (value ? "Hoch" : "Niedrig");
        snprintf(line, sizeof(line), "%s\n%s", names[i], state_text);
        ui_label_set_text_if_changed(toggle_labels[i], line);
        bool highlight = s.fresh && value;
        ui_bg_color_if_changed(toggles[i], p.active ? lv_color_hex(UI_COLOR_WARNING) : highlight ? ui_theme_primary() : ui_theme_card());
        ui_text_color_if_changed(toggle_labels[i], p.active || highlight ? ui_theme_btn_active_text() : ui_theme_btn_text());
        enable(toggles[i], s.fresh && !p.active);
    }
}
