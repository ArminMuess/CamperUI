#include "ui_main.h"
#include "http_handler.h"
#include "ui_chart_style.h"

static lv_obj_t *lbl_indoor;
static lv_obj_t *lbl_outdoor;
static lv_obj_t *lbl_target;
static lv_obj_t *arc_target;
static lv_obj_t *btn_temp_minus = nullptr;
static lv_obj_t *btn_temp_plus = nullptr;
static lv_obj_t *lbl_btn_temp_minus = nullptr;
static lv_obj_t *lbl_btn_temp_plus = nullptr;
static lv_obj_t *btn_start_stop;
static lv_obj_t *lbl_btn_start_stop;
static lv_obj_t *dd_mode;
static lv_obj_t *card_webasto = nullptr;
static lv_obj_t *lbl_webasto_temp = nullptr;
static lv_obj_t *lbl_webasto_volt = nullptr;
static lv_obj_t *lbl_heater_status;
static int last_heating_state = -1;

#define HEATER_HOLD_MS 5000

static lv_obj_t *diagnostics_overlay = nullptr;
static lv_obj_t *diagnostics_values[4] = {};
static lv_obj_t *diagnostics_status = nullptr, *diagnostics_error = nullptr, *diagnostics_notice = nullptr;

static void close_diagnostics(lv_event_t *) {
    if (!diagnostics_overlay) return;
    lv_obj_del_async(diagnostics_overlay);
    diagnostics_overlay = nullptr;
    for (auto &value : diagnostics_values) value = nullptr;
    diagnostics_status = diagnostics_error = diagnostics_notice = nullptr;
}
static void update_diagnostics();
static lv_obj_t *diagnostic_label(lv_obj_t *parent, const char *caption, int x, int y, int width, const lv_font_t *font) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, caption); lv_obj_set_pos(label, x, y); lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0); lv_obj_set_style_text_color(label, ui_theme_text(), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}
static void open_diagnostics(lv_event_t *) {
    if (diagnostics_overlay) return;
    lv_mem_monitor_t memory; lv_mem_monitor(&memory);
    if (memory.free_biggest_size < 32768) {
        lv_obj_t *box = lv_msgbox_create(nullptr, "Diagnose", "Zu wenig freier Speicher.", nullptr, true);
        if (box) lv_obj_center(box);
        return;
    }
    // Opaque modal avoids the large opacity buffers used by translucent full-screen containers.
    diagnostics_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(diagnostics_overlay);
    lv_obj_set_size(diagnostics_overlay, 480, 480); lv_obj_set_pos(diagnostics_overlay, 0, 0);
    lv_obj_set_style_bg_color(diagnostics_overlay, ui_theme_bg(), 0);
    lv_obj_set_style_bg_opa(diagnostics_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(diagnostics_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(diagnostics_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *panel = ui_create_card(diagnostics_overlay, 456, 440);
    lv_obj_center(panel); lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *back = lv_btn_create(panel);
    lv_obj_set_pos(back, 12, 12); lv_obj_set_size(back, 108, 44);
    ui_chart_button_style(back, false);
    lv_obj_add_event_cb(back, close_diagnostics, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *caption = lv_label_create(back); lv_label_set_text(caption, LV_SYMBOL_LEFT " Zurück");
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_14, 0); lv_obj_center(caption);
    diagnostic_label(panel, "Autoterm Diagnose", 134, 25, 310, &lv_font_montserrat_18);
    diagnostics_status = diagnostic_label(panel, "Warte auf Daten...", 16, 70, 424, &lv_font_montserrat_16);
    lv_obj_set_height(diagnostics_status, 44);
    const char *names[] = {"Spannung", "Lüfterdrehzahl", "Kraftstoffpumpe", "Heizgerätetemperatur"};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *card = ui_create_card(panel, 212, 84);
        lv_obj_set_pos(card, 12 + (i % 2) * 220, 124 + (i / 2) * 96);
        lv_obj_set_style_pad_all(card, 0, 0);
        diagnostic_label(card, names[i], 10, 10, 192, &lv_font_montserrat_14);
        diagnostics_values[i] = diagnostic_label(card, "--", 10, 39, 192, &lv_font_montserrat_24);
        lv_obj_set_style_text_color(diagnostics_values[i], lv_color_hex(UI_COLOR_PRIMARY), 0);
    }
    diagnostics_error = diagnostic_label(panel, "", 16, 320, 424, &lv_font_montserrat_14);
    lv_obj_set_height(diagnostics_error, 48);
    diagnostics_notice = diagnostic_label(panel, "", 16, 380, 424, &lv_font_montserrat_12);
    update_diagnostics();
}

// Translate the raw Autoterm "heatstatus" text from VanPi into a short German label.
// Unknown values are shown as-is.
static String heater_status_text(const String &raw) {
    String s = raw; s.trim();
    String l = s; l.toLowerCase();
    if (l.length() == 0 || l == "wait")         return "Warte auf Daten...";
    if (l == "standby" || l == "heater off")    return "Standby";
    if (l == "heating" || l == "running")       return "Heizt";
    if (l == "ventilation" || l == "only fan")  return "Lueftet";
    if (l.indexOf("glow plug") >= 0)            return "Gluehkerze vorheizen";
    if (l.indexOf("ignition 1") >= 0)           return "Zuendung 1";
    if (l.indexOf("ignition 2") >= 0)           return "Zuendung 2";
    if (l == "starting")                        return "Startet";
    if (l == "warming up")                      return "Aufwaermen";
    if (l == "cooling flame sensor")            return "Flammsensor kuehlt";
    if (l == "cooling down")                    return "Abkuehlen";
    if (l == "shutting down")                   return "Faehrt herunter";
    if (l == "flame-out")                       return "Flammabriss!";
    if (l == "no ignition error")               return "Fehler: Keine Zuendung";
    if (l.indexOf("no fuel") >= 0)              return "Kein Kraftstoff? Neuer Versuch";
    if (l == "unknown status")                  return "Status unbekannt";
    return s;
}

static void update_diagnostics() {
    if (!diagnostics_overlay) return;
    StateLockGuard lock;
    const auto &diag = state.heater_diagnostics;
    const bool same_source = state.data_is_demo == state.debug_mode && diag.demo == state.debug_mode;
    const bool connected = state.wifi_connected && state.vanpi_connected;
    const bool fresh = same_source && diag.fresh(millis(), state.debug_mode, connected);
    const char *formats[] = {"%.1f V", "%.0f U/min", "%.2f Hz", "%.0f °C"};
    for (int i = 0; i < 4; ++i) {
        if (fresh && (diag.fields & (1U << i))) ui_label_set_float(diagnostics_values[i], formats[i], diag.values[i]);
        else ui_label_set_text_if_changed(diagnostics_values[i], "--");
    }
    String status = fresh ? heater_status_text(state.heater_status) : "Keine aktuellen Heizungsdaten";
    ui_label_set_text_if_changed(diagnostics_status, status.c_str());
    String error = state.heater_error; error.trim();
    bool no_error = error.equalsIgnoreCase("no") || error == "0" || error.equalsIgnoreCase("none");
    String error_text = !fresh || !error.length() || error.equalsIgnoreCase("null") ? "Fehlerstatus: --" :
                        no_error ? "Fehlerstatus: Kein Fehler gemeldet" : "Fehler: " + error;
    ui_label_set_text_if_changed(diagnostics_error, error_text.c_str());
    ui_text_color_if_changed(diagnostics_error, fresh && !no_error && error.length() && !error.equalsIgnoreCase("null") ?
                                lv_color_hex(UI_COLOR_DANGER) : ui_theme_muted());
    char notice[160];
    if (!same_source || !diag.received) snprintf(notice, sizeof(notice), "Warte auf Autoterm-1-Daten vom VanPi.");
    else if (!state.debug_mode && !connected) snprintf(notice, sizeof(notice), "VanPi nicht verbunden. Letzte Werte werden ausgeblendet.");
    else if (!fresh) snprintf(notice, sizeof(notice), "VanPi-Antwort veraltet (über 45 s). Messwerte ausgeblendet.");
    else snprintf(notice, sizeof(notice), "%s | VanPi-Antwort vor %lu s\n%s",
                  state.debug_mode ? "DEMO / Beispielwerte" : "LIVE / Autoterm 1",
                  (unsigned long)((millis() - diag.updated) / 1000),
                  diag.fields == 15 ? "Werte werden automatisch aktualisiert." : "-- = Messwert nicht verfügbar.");
    ui_label_set_text_if_changed(diagnostics_notice, notice);
}

static void update_status_label() {
    if (!lbl_heater_status) return;
    String txt = heater_status_text(state.heater_status);
    String err = state.heater_error; err.trim();
    bool has_err = err.length() > 0 && err != "no" && err != "0" && err != "null";
    if (has_err) txt += " | Fehler: " + err;

    if (strcmp(lv_label_get_text(lbl_heater_status), txt.c_str()) != 0) {
        lv_label_set_text(lbl_heater_status, txt.c_str());
    }

    String l = state.heater_status; l.toLowerCase();
    lv_color_t c = ui_theme_muted();
    if (has_err || l.indexOf("error") >= 0 || l.indexOf("flame-out") >= 0 || l.indexOf("no fuel") >= 0) {
        c = lv_color_hex(UI_COLOR_DANGER);
    } else if (l == "heating" || l == "running" || l.indexOf("ignition") >= 0 || l.indexOf("glow") >= 0 ||
               l == "starting" || l == "warming up") {
        c = lv_color_hex(UI_COLOR_WARNING);
    } else if (l == "ventilation" || l == "only fan") {
        c = lv_color_hex(UI_COLOR_PRIMARY);
    }
    lv_obj_set_style_text_color(lbl_heater_status, c, 0);
}

static void update_arc_color(lv_obj_t *arc) {
    int val = lv_arc_get_value(arc);
    int pct = 0;
    
    if (state.heater_vent_mode) {
        pct = 0;
    } else {
        if (state.heater_power_mode) {
            pct = ((val - 1) * 255) / 9;
        } else {
            pct = ((val - 10) * 255) / 25;
        }
    }
    
    if (pct < 0) pct = 0;
    if (pct > 255) pct = 255;
    
    lv_color_t color;
    if (state.heater_vent_mode) {
        color = lv_color_hex(UI_COLOR_PRIMARY); // Blue for vent
    } else {
        color = lv_color_mix(lv_color_hex(UI_COLOR_DANGER), lv_color_hex(UI_COLOR_PRIMARY), pct);
    }
    
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, lv_color_hex(0xffffff), LV_PART_KNOB);
    lv_obj_set_style_border_color(arc, color, LV_PART_KNOB);
    lv_obj_set_style_border_width(arc, 4, LV_PART_KNOB);
}

static void update_ui_from_mode() {
    if (is_webasto_heater(state.heater_type)) {
        if (dd_mode) lv_obj_add_flag(dd_mode, LV_OBJ_FLAG_HIDDEN);
        if (card_webasto) lv_obj_clear_flag(card_webasto, LV_OBJ_FLAG_HIDDEN);

        lv_arc_set_range(arc_target, 10, 35);
        int t = (int)(state.target_temp + 0.5f);
        if (t < 10) t = 20;
        if (t > 35) t = 35;
        lv_arc_set_value(arc_target, t);
        lv_label_set_text_fmt(lbl_target, "Ziel: %d C", t);
        update_arc_color(arc_target);

        if (state.heating_on) {
            lv_obj_set_style_bg_color(btn_start_stop, lv_color_hex(UI_COLOR_DANGER), 0);
            lv_obj_set_style_border_color(btn_start_stop, lv_color_hex(0xf87171), 0);
            ui_label_set_text_if_changed(lbl_btn_start_stop, "Stop");
        } else {
            lv_obj_set_style_bg_color(btn_start_stop, lv_color_hex(UI_COLOR_SUCCESS), 0);
            lv_obj_set_style_border_color(btn_start_stop, lv_color_hex(0x34d399), 0);
            ui_label_set_text_if_changed(lbl_btn_start_stop, "Start");
        }
        return;
    }

    if (card_webasto) lv_obj_add_flag(card_webasto, LV_OBJ_FLAG_HIDDEN);
    if (dd_mode) lv_obj_clear_flag(dd_mode, LV_OBJ_FLAG_HIDDEN);

    if (state.heater_vent_mode) {
        lv_dropdown_set_selected(dd_mode, 2);
        lv_arc_set_range(arc_target, 1, 10);
        lv_arc_set_value(arc_target, state.heater_power_level > 0 ? state.heater_power_level : 1);
        lv_label_set_text_fmt(lbl_target, "Luefter Stufe: %d", lv_arc_get_value(arc_target));
    } else if (state.heater_power_mode) {
        lv_dropdown_set_selected(dd_mode, 1);
        lv_arc_set_range(arc_target, 1, 10);
        lv_arc_set_value(arc_target, state.heater_power_level > 0 ? state.heater_power_level : 1);
        lv_label_set_text_fmt(lbl_target, "Heiz Stufe: %d", lv_arc_get_value(arc_target));
    } else {
        lv_dropdown_set_selected(dd_mode, 0);
        lv_arc_set_range(arc_target, 10, 35);
        int t = (int)(state.target_temp + 0.5f);
        if (t < 10) t = 20;
        if (t > 35) t = 35;
        lv_arc_set_value(arc_target, t);
        lv_label_set_text_fmt(lbl_target, "Ziel: %d C", t);
    }
    update_arc_color(arc_target);
    
    if (state.heating_on) {
        lv_obj_set_style_bg_color(btn_start_stop, lv_color_hex(UI_COLOR_DANGER), 0);
        lv_obj_set_style_border_color(btn_start_stop, lv_color_hex(0xf87171), 0);
        ui_label_set_text_if_changed(lbl_btn_start_stop, "Stop");
    } else {
        lv_obj_set_style_bg_color(btn_start_stop, lv_color_hex(UI_COLOR_SUCCESS), 0);
        lv_obj_set_style_border_color(btn_start_stop, lv_color_hex(0x34d399), 0);
        ui_label_set_text_if_changed(lbl_btn_start_stop, "Start");
    }
}

static void dd_mode_event_cb(lv_event_t * e) {
    lv_obj_t * dd = lv_event_get_target(e);
    int sel = lv_dropdown_get_selected(dd);
    state.heater_hold_until = millis() + HEATER_HOLD_MS;
    
    if (sel == 0) { state.heater_vent_mode = false; state.heater_power_mode = false; }
    else if (sel == 1) { state.heater_vent_mode = false; state.heater_power_mode = true; }
    else if (sel == 2) { state.heater_vent_mode = true; state.heater_power_mode = false; }
    
    update_ui_from_mode();

    // Heater already running -> switch mode immediately
    if (state.heating_on) {
        String m = "temp";
        int val = (int)state.target_temp;
        if (state.heater_vent_mode) { m = "vent"; val = state.heater_power_level; }
        else if (state.heater_power_mode) { m = "power"; val = state.heater_power_level; }
        http_publish_heater_cmd(m, val);
    }
}

static void btn_start_stop_event_cb(lv_event_t * e) {
    state.heater_hold_until = millis() + HEATER_HOLD_MS;
    if (is_webasto_heater(state.heater_type)) {
        if (state.heating_on) {
            http_publish_webasto(false, 0);
            state.heating_on = false;
        } else {
            int val = (int)(state.target_temp + 0.5f);
            if (val < 10) val = 20;
            http_publish_webasto(true, val);
            state.heating_on = true;
        }
        update_ui_from_mode();
        return;
    }

    if (state.heating_on) {
        http_publish_heater_cmd("stop", 0);
        state.heating_on = false;
    } else {
        String m = "temp";
        int val = (int)state.target_temp;
        if (state.heater_vent_mode) { m = "vent"; val = state.heater_power_level; }
        else if (state.heater_power_mode) { m = "power"; val = state.heater_power_level; }
        
        http_publish_heater_cmd(m, val);
        state.heating_on = true;
    }
    update_ui_from_mode();
}

static void arc_target_event_cb(lv_event_t * e) {
    lv_obj_t * arc = lv_event_get_target(e);
    int val = lv_arc_get_value(arc);
    state.heater_hold_until = millis() + HEATER_HOLD_MS;
    
    if (is_webasto_heater(state.heater_type)) {
        state.target_temp = val;
        lv_label_set_text_fmt(lbl_target, "Ziel: %d C", val);
        update_arc_color(arc);
        if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
            if (state.heating_on) {
                http_publish_webasto(true, val);
            }
        }
        return;
    }

    if (state.heater_vent_mode) {
        state.heater_power_level = val;
        lv_label_set_text_fmt(lbl_target, "Luefter Stufe: %d", val);
    } else if (state.heater_power_mode) {
        state.heater_power_level = val;
        lv_label_set_text_fmt(lbl_target, "Heiz Stufe: %d", val);
    } else {
        state.target_temp = val;
        lv_label_set_text_fmt(lbl_target, "Ziel: %d C", val);
    }
    
    update_arc_color(arc);
    
    if(lv_event_get_code(e) == LV_EVENT_RELEASED) {
        if (state.heating_on) {
            String m = "temp";
            if (state.heater_vent_mode) m = "vent";
            else if (state.heater_power_mode) m = "power";
            http_publish_heater_cmd(m, val);
        }
    }
}

static void btn_temp_adjust_event_cb(lv_event_t *e) {
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    state.heater_hold_until = millis() + HEATER_HOLD_MS;

    if (is_webasto_heater(state.heater_type)) {
        int cur = (int)(state.target_temp + 0.5f);
        if (cur < 10) cur = 20;
        cur = constrain(cur + delta, 10, 35);
        state.target_temp = cur;
        if (arc_target) {
            lv_arc_set_value(arc_target, cur);
            update_arc_color(arc_target);
        }
        if (lbl_target) {
            lv_label_set_text_fmt(lbl_target, "Ziel: %d C", cur);
        }
        if (state.heating_on) {
            http_publish_webasto(true, cur);
        }
        return;
    }

    if (state.heater_vent_mode || state.heater_power_mode) {
        int cur = state.heater_power_level > 0 ? state.heater_power_level : 1;
        cur = constrain(cur + delta, 1, 10);
        state.heater_power_level = cur;
        if (arc_target) {
            lv_arc_set_value(arc_target, cur);
            update_arc_color(arc_target);
        }
        if (lbl_target) {
            lv_label_set_text_fmt(lbl_target, state.heater_vent_mode ? "Luefter Stufe: %d" : "Heiz Stufe: %d", cur);
        }
        if (state.heating_on) {
            http_publish_heater_cmd(state.heater_vent_mode ? "vent" : "power", cur);
        }
    } else {
        int cur = (int)(state.target_temp + 0.5f);
        if (cur < 10) cur = 20;
        cur = constrain(cur + delta, 10, 35);
        state.target_temp = cur;
        if (arc_target) {
            lv_arc_set_value(arc_target, cur);
            update_arc_color(arc_target);
        }
        if (lbl_target) {
            lv_label_set_text_fmt(lbl_target, "Ziel: %d C", cur);
        }
        if (state.heating_on) {
            http_publish_heater_cmd("temp", cur);
        }
    }
}

void ui_update_climate_tab() {
    if (!lbl_indoor) return;
    
    ui_label_set_float(lbl_indoor, "%.1f C", state.indoor_temp);
    if (lbl_outdoor) {
        ui_label_set_float(lbl_outdoor, "%.1f C", state.outdoor_temp);
    }

    update_status_label();
    update_diagnostics();

    if (is_webasto_heater(state.heater_type)) {
        if (lbl_webasto_temp) {
            if (state.webasto_temp > 0.0f) {
                char buf[32];
                snprintf(buf, sizeof(buf), "Heizer: %.0f C", state.webasto_temp);
                ui_label_set_text_if_changed(lbl_webasto_temp, buf);
            } else {
                ui_label_set_text_if_changed(lbl_webasto_temp, "Heizer: -- C");
            }
        }
        if (lbl_webasto_volt) {
            if (state.webasto_voltage > 5.0f) {
                char buf[32];
                snprintf(buf, sizeof(buf), "Spannung: %.1f V", state.webasto_voltage);
                ui_label_set_text_if_changed(lbl_webasto_volt, buf);
            } else {
                ui_label_set_text_if_changed(lbl_webasto_volt, "Spannung: --.- V");
            }
        }
    }

    if (btn_temp_minus && btn_temp_plus) {
        if (state.heater_temp_buttons) {
            lv_obj_clear_flag(btn_temp_minus, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(btn_temp_plus, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(btn_temp_minus, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_temp_plus, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Don't touch controls while the user is interacting with them
    if (lv_obj_has_state(arc_target, LV_STATE_PRESSED) || (!is_webasto_heater(state.heater_type) && lv_dropdown_is_open(dd_mode))) return;

    if (is_webasto_heater(state.heater_type)) {
        int expected_val = (int)(state.target_temp + 0.5f);
        if (expected_val < 10) expected_val = 20;
        if (expected_val > 35) expected_val = 35;
        if (lv_arc_get_value(arc_target) != expected_val) {
            update_ui_from_mode();
            last_heating_state = (int)state.heating_on;
        } else if ((int)state.heating_on != last_heating_state) {
            last_heating_state = (int)state.heating_on;
            if (state.heating_on) {
                lv_obj_set_style_bg_color(btn_start_stop, lv_color_hex(UI_COLOR_DANGER), 0);
                lv_obj_set_style_border_color(btn_start_stop, lv_color_hex(0xf87171), 0);
                ui_label_set_text_if_changed(lbl_btn_start_stop, "Stop");
            } else {
                lv_obj_set_style_bg_color(btn_start_stop, lv_color_hex(UI_COLOR_SUCCESS), 0);
                lv_obj_set_style_border_color(btn_start_stop, lv_color_hex(0x34d399), 0);
                ui_label_set_text_if_changed(lbl_btn_start_stop, "Start");
            }
        }
        return;
    }
    
    int expected_sel = 0;
    if (state.heater_vent_mode) expected_sel = 2;
    else if (state.heater_power_mode) expected_sel = 1;

    int expected_val;
    if (expected_sel == 0) {
        expected_val = (int)(state.target_temp + 0.5f);
        if (expected_val < 10) expected_val = 20;
        if (expected_val > 35) expected_val = 35;
    } else {
        expected_val = constrain(state.heater_power_level, 1, 10);
    }
    
    bool arc_needs_update = false;
    if (expected_sel != (int)lv_dropdown_get_selected(dd_mode)) arc_needs_update = true;
    if (lv_arc_get_value(arc_target) != expected_val) arc_needs_update = true;
    
    if (arc_needs_update) {
        update_ui_from_mode();
        last_heating_state = (int)state.heating_on;
    } else {
        if ((int)state.heating_on != last_heating_state) {
            last_heating_state = (int)state.heating_on;
            if (state.heating_on) {
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

void ui_climate_reset() {
    close_diagnostics(nullptr);
    lbl_indoor = nullptr;
    lbl_outdoor = nullptr;
    lbl_target = nullptr;
    arc_target = nullptr;
    btn_temp_minus = nullptr;
    btn_temp_plus = nullptr;
    lbl_btn_temp_minus = nullptr;
    lbl_btn_temp_plus = nullptr;
    btn_start_stop = nullptr;
    lbl_btn_start_stop = nullptr;
    dd_mode = nullptr;
    card_webasto = nullptr;
    lbl_webasto_temp = nullptr;
    lbl_webasto_volt = nullptr;
    lbl_heater_status = nullptr;
    last_heating_state = -1;
}

void ui_build_climate(lv_obj_t *parent) {
    ui_climate_reset();
    ui_setup_tab_page(parent);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // ==========================================
    // 1. Top Row: Outdoor Temp (Left) & Mode Dropdown (Right)
    // ==========================================
    // Left Badge: Outdoor Temp (as user liked before)
    lv_obj_t *card_out = ui_create_card(parent, 150, 58);
    lv_obj_align(card_out, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_all(card_out, 8, 0);
    
    lv_obj_t *l_out_sub = lv_label_create(card_out);
    lv_label_set_text_fmt(l_out_sub, "%s Außen", LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(l_out_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(l_out_sub, ui_theme_muted(), 0);
    lv_obj_align(l_out_sub, LV_ALIGN_TOP_LEFT, 4, 0);

    lbl_outdoor = lv_label_create(card_out);
    lv_label_set_text(lbl_outdoor, "--.- C");
    lv_obj_set_style_text_font(lbl_outdoor, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_outdoor, ui_theme_text(), 0);
    lv_obj_align(lbl_outdoor, LV_ALIGN_BOTTOM_LEFT, 4, -2);

    // Right: Mode Dropdown
    dd_mode = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd_mode, "Heizen (Temp)\nHeizen (Stufe)\nLueften");
    lv_obj_set_size(dd_mode, 190, 58);
    lv_obj_align(dd_mode, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_radius(dd_mode, 14, 0);
    lv_obj_set_style_bg_color(dd_mode, ui_theme_card(), 0);
    lv_obj_set_style_border_color(dd_mode, ui_theme_border(), 0);
    lv_obj_set_style_border_width(dd_mode, 1, 0);
    lv_obj_set_style_text_color(dd_mode, ui_theme_text(), 0);
    lv_obj_set_style_text_font(dd_mode, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_top(dd_mode, 19, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(dd_mode, 19, LV_PART_MAIN);
    lv_obj_set_style_pad_left(dd_mode, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_right(dd_mode, 36, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dd_mode, 0, 0);
    
    lv_obj_t * list = lv_dropdown_get_list(dd_mode);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_18, 0);
    lv_obj_set_style_pad_all(list, 10, 0);
    lv_obj_set_style_bg_color(list, ui_theme_card(), 0);
    lv_obj_set_style_border_color(list, ui_theme_border(), 0);
    lv_obj_add_event_cb(dd_mode, dd_mode_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Right: Webasto Telemetry Card (shown when Webasto is active)
    card_webasto = ui_create_card(parent, 190, 58);
    lv_obj_align(card_webasto, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_pad_all(card_webasto, 8, 0);

    lbl_webasto_temp = lv_label_create(card_webasto);
    lv_label_set_text(lbl_webasto_temp, "Heizer: -- C");
    lv_obj_set_style_text_font(lbl_webasto_temp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_webasto_temp, ui_theme_text(), 0);
    lv_obj_align(lbl_webasto_temp, LV_ALIGN_TOP_LEFT, 4, 2);

    lbl_webasto_volt = lv_label_create(card_webasto);
    lv_label_set_text(lbl_webasto_volt, "Spannung: --.- V");
    lv_obj_set_style_text_font(lbl_webasto_volt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_webasto_volt, ui_theme_muted(), 0);
    lv_obj_align(lbl_webasto_volt, LV_ALIGN_BOTTOM_LEFT, 4, -2);

    if (is_webasto_heater(state.heater_type)) {
        lv_obj_add_flag(dd_mode, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(card_webasto, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(dd_mode, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card_webasto, LV_OBJ_FLAG_HIDDEN);
    }

    // ==========================================
    // 2. Thermostat Arc Gauge
    // ==========================================
    arc_target = lv_arc_create(parent);
    lv_obj_set_size(arc_target, 230, 230);
    lv_obj_align(arc_target, LV_ALIGN_TOP_MID, 0, 56);
    
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

    // Indoor Temp Label (Dead-centered)
    lbl_indoor = lv_label_create(parent);
    lv_obj_set_width(lbl_indoor, 200);
    lv_obj_set_style_text_align(lbl_indoor, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_indoor, "--.- C");
    lv_obj_set_style_text_font(lbl_indoor, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(lbl_indoor, ui_theme_text(), 0);
    lv_obj_align_to(lbl_indoor, arc_target, LV_ALIGN_CENTER, 0, -14);
    
    // Target Value Label (Dead-centered)
    lbl_target = lv_label_create(parent);
    lv_obj_set_width(lbl_target, 200);
    lv_obj_set_style_text_align(lbl_target, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_target, "Ziel: -- C");
    lv_obj_set_style_text_font(lbl_target, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_target, ui_theme_muted(), 0);
    lv_obj_align_to(lbl_target, arc_target, LV_ALIGN_CENTER, 0, 26);

    // Heater Status Text (below the arc, e.g. "Standby", "Heizt", "Zuendung 1")
    lbl_heater_status = lv_label_create(parent);
    lv_obj_set_width(lbl_heater_status, 400);
    lv_label_set_long_mode(lbl_heater_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_heater_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_heater_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_heater_status, ui_theme_muted(), 0);
    lv_label_set_text(lbl_heater_status, "");
    // Arc bottom = 56 + 230 = 286; the arc ends + knob reach down to ~267,
    // the Start/Stop button starts at 306 -> place label at ~268..288.
    lv_obj_align_to(lbl_heater_status, arc_target, LV_ALIGN_OUT_BOTTOM_MID, 0, -18);
    update_status_label();

    // ==========================================
    // 3. Controls (Bottom Row: Centered Start/Stop Button)
    // ==========================================
    btn_start_stop = lv_btn_create(parent);
    lv_obj_set_size(btn_start_stop, 260, 54);
    lv_obj_align(btn_start_stop, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(btn_start_stop, 14, 0);
    lv_obj_set_style_border_width(btn_start_stop, 1, 0);
    lv_obj_set_style_shadow_width(btn_start_stop, 0, 0);
    lv_obj_add_event_cb(btn_start_stop, btn_start_stop_event_cb, LV_EVENT_CLICKED, NULL);
    
    lbl_btn_start_stop = lv_label_create(btn_start_stop);
    lv_label_set_text(lbl_btn_start_stop, "Start");
    lv_obj_set_style_text_font(lbl_btn_start_stop, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_btn_start_stop);
    
    // Read-only diagnosis, directly beside the Start/Stop control.
    lv_obj_t *diagnosis = lv_btn_create(parent);
    lv_obj_set_size(diagnosis, 54, 54);
    lv_obj_align_to(diagnosis, btn_start_stop, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
    ui_chart_button_style(diagnosis, false);
    lv_obj_set_style_radius(diagnosis, 14, 0); lv_obj_set_style_pad_all(diagnosis, 0, 0);
    lv_obj_add_event_cb(diagnosis, open_diagnostics, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *diagnosis_icon = lv_label_create(diagnosis);
    lv_label_set_text(diagnosis_icon, MDI_STETHOSCOPE);
    lv_obj_set_style_text_font(diagnosis_icon, &ui_font_mdi_32, 0);
    lv_obj_set_style_text_color(diagnosis_icon, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_center(diagnosis_icon);

    last_heating_state = -1;
    update_ui_from_mode();
}
