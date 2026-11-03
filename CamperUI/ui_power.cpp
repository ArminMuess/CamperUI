#include "ui_main.h"
#include "ui_energy_history.h"
#include <stdio.h>
#include <math.h>

// =========================================================================
// UI Pointers - CamperUI Energy Flow Dashboard V6
// =========================================================================

// 1. Produzenten (Top Row, Höhe 74px)
static lv_obj_t *arc_solar = nullptr;
static lv_obj_t *lbl_solar_pct = nullptr;
static lv_obj_t *lbl_solar_w = nullptr;
static lv_obj_t *lbl_solar_sub = nullptr;

static lv_obj_t *arc_shore = nullptr;
static lv_obj_t *lbl_shore_pct = nullptr;
static lv_obj_t *lbl_shore_w = nullptr;
static lv_obj_t *lbl_shore_sub = nullptr;

static lv_obj_t *arc_alt = nullptr;
static lv_obj_t *lbl_alt_pct = nullptr;
static lv_obj_t *lbl_alt_w = nullptr;
static lv_obj_t *lbl_alt_sub = nullptr;

// 2. Fluss-Linien (Top -> Center)
static lv_obj_t *line_solar = nullptr;
static lv_obj_t *line_shore = nullptr;
static lv_obj_t *line_alt = nullptr;

// 3. Zentrale Einheit: Batterie (Center: 132px, Arc vertikal mittig, % mittig im Arc, Spannung unter Watt/A)
static lv_obj_t *card_bat_hub = nullptr;
static lv_obj_t *arc_battery = nullptr;
static lv_obj_t *lbl_bat_pct = nullptr;
static lv_obj_t *lbl_bat_volt = nullptr;
static lv_obj_t *lbl_bat_watt = nullptr;
static lv_obj_t *lbl_bat_amp = nullptr;

// 4. Fluss-Linie: Center -> Bottom
static lv_obj_t *line_load = nullptr;

// 5. Verbraucher (Bottom: Höhe 74px wie Erzeuger, weiter unten & zentriert)
static lv_obj_t *card_load = nullptr;
static lv_obj_t *lbl_load_watt = nullptr;
static lv_obj_t *lbl_load_sub = nullptr;

// Statische Koordinaten für LVGL-Linien (LVGL speichert Zeiger auf Punkte)
static lv_point_t pts_solar[3];
static lv_point_t pts_shore[3];
static lv_point_t pts_alt[3];
static lv_point_t pts_load[2] = {{220, 246}, {220, 286}};

// Animation & Caching
static int last_soc = -1;
static int last_shunt_dir = -99;

static void set_arc_val_cb(void * var, int32_t v) {
    lv_arc_set_value((lv_obj_t*)var, v);
}

void ui_trigger_power_anim() {
    last_soc = -1;
    last_shunt_dir = -99;
    if (arc_battery) lv_arc_set_value(arc_battery, 0);
    if (arc_solar) lv_arc_set_value(arc_solar, 0);
    if (arc_shore) lv_arc_set_value(arc_shore, 0);
    if (arc_alt) lv_arc_set_value(arc_alt, 0);
}

void ui_power_reset() {
    if (arc_battery) lv_anim_del(arc_battery, set_arc_val_cb);
    arc_battery = nullptr;
    lbl_bat_pct = nullptr;
    lbl_bat_volt = nullptr;
    lbl_bat_watt = nullptr;
    lbl_bat_amp = nullptr;
    card_bat_hub = nullptr;

    arc_solar = nullptr;
    lbl_solar_pct = nullptr;
    lbl_solar_w = nullptr;
    lbl_solar_sub = nullptr;

    arc_shore = nullptr;
    lbl_shore_pct = nullptr;
    lbl_shore_w = nullptr;
    lbl_shore_sub = nullptr;

    arc_alt = nullptr;
    lbl_alt_pct = nullptr;
    lbl_alt_w = nullptr;
    lbl_alt_sub = nullptr;

    line_solar = nullptr;
    line_shore = nullptr;
    line_alt = nullptr;
    line_load = nullptr;

    card_load = nullptr;
    lbl_load_watt = nullptr;
    lbl_load_sub = nullptr;

    last_soc = -1;
    last_shunt_dir = -99;
}

static bool last_shore_shown = false;
static bool last_alt_shown = false;

void ui_update_power_tab() {
    if (!arc_battery) return;

    bool cur_shore_shown = state.power_show_shore && state.shore_available;
    bool cur_alt_shown = state.power_show_alt && state.alt_available;
    if (cur_shore_shown != last_shore_shown || cur_alt_shown != last_alt_shown) {
        last_shore_shown = cur_shore_shown;
        last_alt_shown = cur_alt_shown;
        ui_rebuild_power();
        return;
    }

    // -------------------------------------------------------------
    // 1. Sensordaten & elektrische Bilanz berechnen
    // -------------------------------------------------------------
    float v_bat = (state.bat_voltage > 5.0f && isfinite(state.bat_voltage)) ? state.bat_voltage : 13.2f;
    float i_bat = isfinite(state.bat_current) ? state.bat_current : 0.0f;
    float p_bat = v_bat * i_bat; // + = Laden, - = Entladen
    int soc = constrain(state.bat_soc, 0, 100);

    // Solar
    float p_solar = state.power_show_solar ? ((state.solar_power >= 0.0f && isfinite(state.solar_power)) ? state.solar_power : 0.0f) : 0.0f;
    float v_solar = (state.solar_voltage > 1.0f && isfinite(state.solar_voltage)) ? state.solar_voltage : (p_solar > 5.0f ? 18.2f : 0.0f);
    float i_solar = (state.solar_current > 0.05f && isfinite(state.solar_current)) ? state.solar_current : (v_solar > 1.0f ? (p_solar / v_solar) : 0.0f);

    // Landstrom (230V) - Nur aus Telemetrie/Endpoints, keine interne Schätzung
    bool shore_active = cur_shore_shown && (state.shore_connected || state.shore_power > 5.0f);
    float p_shore = cur_shore_shown ? ((state.shore_power > 0.0f && isfinite(state.shore_power)) ? state.shore_power : 0.0f) : 0.0f;
    float v_shore = (state.shore_voltage > 50.0f && isfinite(state.shore_voltage)) ? state.shore_voltage : (shore_active ? 230.0f : 0.0f);
    float i_shore = (state.shore_current > 0.01f && isfinite(state.shore_current)) ? state.shore_current : (v_shore > 50.0f && p_shore > 0.0f ? (p_shore / v_shore) : 0.0f);

    // Lichtmaschine / B2B Booster - Nur aus Telemetrie/Endpoints, keine interne Schätzung
    bool alt_active = cur_alt_shown && (state.alt_active || state.alt_power > 5.0f);
    float p_alt = cur_alt_shown ? ((state.alt_power > 0.0f && isfinite(state.alt_power)) ? state.alt_power : 0.0f) : 0.0f;
    float v_starter = (state.starter_voltage > 1.0f && isfinite(state.starter_voltage)) ? state.starter_voltage : 0.0f;
    float i_alt = (state.alt_current > 0.01f && isfinite(state.alt_current)) ? state.alt_current : ((v_bat > 5.0f && p_alt > 0.0f) ? (p_alt / v_bat) : 0.0f);

    // Verbraucher:
    // Wenn power_calc_load aktiv: Gesamtlast = Erzeuger (Solar + Landstrom + Booster) - Netto-Batterieleistung
    // Wenn inaktiv: Rein aus /batt Telemetrie (Entnahme aus der Batterie)
    float p_load = 0.0f;
    if (state.power_calc_load) {
        float p_gen = p_solar + p_shore + p_alt;
        p_load = p_gen - p_bat;
        if (p_load < 0.0f) p_load = 0.0f;
    } else {
        p_load = (i_bat < -0.05f) ? (-i_bat * v_bat) : 0.0f;
    }

    char buf[48];

    // -------------------------------------------------------------
    // 2. Produzenten (Oben) - Nur aktualisieren wenn vorhanden
    // -------------------------------------------------------------
    if (lbl_solar_w) {
        snprintf(buf, sizeof(buf), "%.0f W", p_solar);
        ui_label_set_text_if_changed(lbl_solar_w, buf);
        snprintf(buf, sizeof(buf), "%.1fV  %.1fA", v_solar, i_solar);
        ui_label_set_text_if_changed(lbl_solar_sub, buf);

        float max_s = (state.solar_max_w > 10.0f) ? state.solar_max_w : 200.0f;
        int pct_s = constrain((int)lroundf((p_solar / max_s) * 100.0f), 0, 100);
        ui_arc_value_if_changed(arc_solar, pct_s);
        if (lbl_solar_pct) {
            snprintf(buf, sizeof(buf), "%d%%", pct_s);
            ui_label_set_text_if_changed(lbl_solar_pct, buf);
            ui_text_color_if_changed(lbl_solar_pct, p_solar > 5.0f ? lv_color_hex(UI_COLOR_WARNING) : ui_theme_muted());
        }
    }

    if (lbl_shore_w) {
        if (!shore_active) {
            ui_label_set_text_if_changed(lbl_shore_w, "Off");
            ui_text_color_if_changed(lbl_shore_w, ui_theme_muted());
            ui_label_set_text_if_changed(lbl_shore_sub, "Getrennt");
            ui_text_color_if_changed(lbl_shore_sub, ui_theme_muted());
            ui_arc_value_if_changed(arc_shore, 0);
            if (lbl_shore_pct) {
                ui_label_set_text_if_changed(lbl_shore_pct, "--");
                ui_text_color_if_changed(lbl_shore_pct, ui_theme_muted());
            }
        } else {
            if (p_shore > 5.0f) snprintf(buf, sizeof(buf), "%.0f W", p_shore);
            else snprintf(buf, sizeof(buf), "0 W");
            ui_label_set_text_if_changed(lbl_shore_w, buf);
            ui_text_color_if_changed(lbl_shore_w, lv_color_hex(UI_COLOR_PRIMARY));

            snprintf(buf, sizeof(buf), "%.0fV  %.1fA", v_shore, i_shore);
            ui_label_set_text_if_changed(lbl_shore_sub, buf);
            ui_text_color_if_changed(lbl_shore_sub, ui_theme_muted());

            float max_sh = (state.shore_max_w > 10.0f) ? state.shore_max_w : 500.0f;
            int pct_sh = constrain((int)lroundf((p_shore / max_sh) * 100.0f), 0, 100);
            ui_arc_value_if_changed(arc_shore, pct_sh);
            if (lbl_shore_pct) {
                snprintf(buf, sizeof(buf), "%d%%", pct_sh);
                ui_label_set_text_if_changed(lbl_shore_pct, buf);
                ui_text_color_if_changed(lbl_shore_pct, p_shore > 5.0f ? lv_color_hex(UI_COLOR_PRIMARY) : ui_theme_muted());
            }
        }
    }

    if (lbl_alt_w) {
        if (!alt_active) {
            ui_label_set_text_if_changed(lbl_alt_w, "Off");
            ui_text_color_if_changed(lbl_alt_w, ui_theme_muted());
            if (v_starter > 1.0f) snprintf(buf, sizeof(buf), "%.1fV Lima", v_starter);
            else snprintf(buf, sizeof(buf), "Inaktiv");
            ui_label_set_text_if_changed(lbl_alt_sub, buf);
            ui_text_color_if_changed(lbl_alt_sub, ui_theme_muted());
            ui_arc_value_if_changed(arc_alt, 0);
            if (lbl_alt_pct) {
                ui_label_set_text_if_changed(lbl_alt_pct, "--");
                ui_text_color_if_changed(lbl_alt_pct, ui_theme_muted());
            }
        } else {
            if (p_alt > 5.0f) snprintf(buf, sizeof(buf), "%.0f W", p_alt);
            else snprintf(buf, sizeof(buf), "0 W");
            ui_label_set_text_if_changed(lbl_alt_w, buf);
            ui_text_color_if_changed(lbl_alt_w, lv_color_hex(0xa855f7));

            if (v_starter > 1.0f) snprintf(buf, sizeof(buf), "%.1fV Lima", v_starter);
            else snprintf(buf, sizeof(buf), "Aktiv");
            ui_label_set_text_if_changed(lbl_alt_sub, buf);
            ui_text_color_if_changed(lbl_alt_sub, ui_theme_muted());

            float max_a = (state.alt_max_w > 10.0f) ? state.alt_max_w : 300.0f;
            int pct_a = constrain((int)lroundf((p_alt / max_a) * 100.0f), 0, 100);
            ui_arc_value_if_changed(arc_alt, pct_a);
            if (lbl_alt_pct) {
                snprintf(buf, sizeof(buf), "%d%%", pct_a);
                ui_label_set_text_if_changed(lbl_alt_pct, buf);
                ui_text_color_if_changed(lbl_alt_pct, p_alt > 5.0f ? lv_color_hex(0xa855f7) : ui_theme_muted());
            }
        }
    }

    // -------------------------------------------------------------
    // 3. Status Laden / Entladen & Linienfarben
    // -------------------------------------------------------------
    // Wenn geladen wird: ALLE aktiven Linien (auch zu den Verbrauchern) werden GRÜN!
    bool is_charging = (i_bat > 0.1f);
    lv_color_t green_c = lv_color_hex(UI_COLOR_SUCCESS);

    if (line_solar) {
        ui_line_color_if_changed(line_solar, p_solar > 5.0f ? (is_charging ? green_c : lv_color_hex(UI_COLOR_WARNING)) : ui_theme_border());
    }
    if (line_shore) {
        ui_line_color_if_changed(line_shore, (shore_active || p_shore > 5.0f) ? (is_charging ? green_c : lv_color_hex(UI_COLOR_PRIMARY)) : ui_theme_border());
    }
    if (line_alt) {
        ui_line_color_if_changed(line_alt, (alt_active || p_alt > 5.0f) ? (is_charging ? green_c : lv_color_hex(0xa855f7)) : ui_theme_border());
    }
    // Linie zu den Verbrauchern (aktiv wenn Strom aus der Batterie entnommen wird)
    if (line_load) {
        ui_line_color_if_changed(line_load, p_load > 2.0f ? lv_color_hex(UI_COLOR_WARNING) : ui_theme_border());
    }

    // -------------------------------------------------------------
    // 4. Zentrale Einheit: Batterie (Mitte) - Großer SoC im Arc, Spannung unter Watt & Ampere
    // -------------------------------------------------------------
    if (soc != last_soc) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, arc_battery);
        lv_anim_set_exec_cb(&a, set_arc_val_cb);
        lv_anim_set_time(&a, 800);
        lv_anim_set_values(&a, lv_arc_get_value(arc_battery), soc);
        lv_anim_start(&a);
        last_soc = soc;
        snprintf(buf, sizeof(buf), "%d%%", soc);
        ui_label_set_text_if_changed(lbl_bat_pct, buf);
    }

    lv_color_t soc_color = ui_battery_soc_color(soc);
    ui_arc_color_if_changed(arc_battery, soc_color, LV_PART_INDICATOR);
    ui_text_color_if_changed(lbl_bat_pct, soc_color);

    // Spannung unter Watt & Ampere
    snprintf(buf, sizeof(buf), "%.1f V", v_bat);
    ui_label_set_text_if_changed(lbl_bat_volt, buf);

    int current_dir = (i_bat > 0.1f) ? 1 : ((i_bat < -0.1f) ? -1 : 0);
    if (current_dir > 0) {
        // Laden (Grün) - Groß die Watt, daneben der Strom
        snprintf(buf, sizeof(buf), "+%.0f W", p_bat);
        ui_label_set_text_if_changed(lbl_bat_watt, buf);
        ui_text_color_if_changed(lbl_bat_watt, green_c);

        snprintf(buf, sizeof(buf), "(+%.1f A)", i_bat);
        ui_label_set_text_if_changed(lbl_bat_amp, buf);
        ui_text_color_if_changed(lbl_bat_amp, green_c);

        if (card_bat_hub) ui_border_color_if_changed(card_bat_hub, green_c);
    } else if (current_dir < 0) {
        // Entladen (Orange/Amber)
        snprintf(buf, sizeof(buf), "%.0f W", p_bat);
        ui_label_set_text_if_changed(lbl_bat_watt, buf);
        ui_text_color_if_changed(lbl_bat_watt, lv_color_hex(UI_COLOR_WARNING));

        snprintf(buf, sizeof(buf), "(%.1f A)", i_bat);
        ui_label_set_text_if_changed(lbl_bat_amp, buf);
        ui_text_color_if_changed(lbl_bat_amp, lv_color_hex(UI_COLOR_WARNING));

        if (card_bat_hub) ui_border_color_if_changed(card_bat_hub, lv_color_hex(UI_COLOR_WARNING));
    } else {
        // Standby (Muted)
        ui_label_set_text_if_changed(lbl_bat_watt, "0 W");
        ui_text_color_if_changed(lbl_bat_watt, ui_theme_text());

        ui_label_set_text_if_changed(lbl_bat_amp, "(0.0 A)");
        ui_text_color_if_changed(lbl_bat_amp, ui_theme_muted());

        if (card_bat_hub) ui_border_color_if_changed(card_bat_hub, ui_theme_border());
    }
    last_shunt_dir = current_dir;

    // -------------------------------------------------------------
    // 5. Verbraucher (Unten) - Nur Leistung & große Restzeit
    // -------------------------------------------------------------
    snprintf(buf, sizeof(buf), "%.0f W", p_load);
    ui_label_set_text_if_changed(lbl_load_watt, buf);
    ui_text_color_if_changed(lbl_load_watt, p_load > 2.0f ? lv_color_hex(UI_COLOR_WARNING) : ui_theme_muted());

    if (i_bat < -0.05f) {
        if (state.time_to_go.length() > 0 && state.time_to_go != "--") {
            snprintf(buf, sizeof(buf), "Restzeit: %s", state.time_to_go.c_str());
        } else {
            snprintf(buf, sizeof(buf), "Entladen");
        }
        ui_label_set_text_if_changed(lbl_load_sub, buf);
        ui_text_color_if_changed(lbl_load_sub, lv_color_hex(UI_COLOR_WARNING));
    } else if (i_bat > 0.05f) {
        if (soc >= 100) {
            ui_label_set_text_if_changed(lbl_load_sub, "Batterie Voll");
        } else {
            ui_label_set_text_if_changed(lbl_load_sub, "Wird geladen");
        }
        ui_text_color_if_changed(lbl_load_sub, green_c);
    } else {
        ui_label_set_text_if_changed(lbl_load_sub, "Standby");
        ui_text_color_if_changed(lbl_load_sub, ui_theme_muted());
    }
}

void ui_build_power(lv_obj_t *parent) {
    ui_power_reset();
    ui_setup_tab_page(parent);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // =========================================================================
    // 1. TIER 1: PRODUZENTEN (Dynamisch zentriert je nach Auswahl in den Settings)
    // =========================================================================
    struct ProdSlot {
        int type; // 0=Solar, 1=Landstrom, 2=Lichtmaschine
        lv_coord_t x;
        lv_coord_t w;
        lv_coord_t cx;
    };
    last_shore_shown = state.power_show_shore && state.shore_available;
    last_alt_shown = state.power_show_alt && state.alt_available;

    ProdSlot slots[3];
    int prod_count = 0;
    if (state.power_show_solar) { slots[prod_count++].type = 0; }
    if (last_shore_shown) { slots[prod_count++].type = 1; }
    if (last_alt_shown)   { slots[prod_count++].type = 2; }

    if (prod_count == 3) {
        slots[0].x = 0;   slots[0].w = 140; slots[0].cx = 70;
        slots[1].x = 150; slots[1].w = 140; slots[1].cx = 220;
        slots[2].x = 300; slots[2].w = 140; slots[2].cx = 370;
    } else if (prod_count == 2) {
        slots[0].x = 12;  slots[0].w = 200; slots[0].cx = 112;
        slots[1].x = 228; slots[1].w = 200; slots[1].cx = 328;
    } else if (prod_count == 1) {
        slots[0].x = 100; slots[0].w = 240; slots[0].cx = 220;
    }

    for (int i = 0; i < prod_count; i++) {
        lv_coord_t x = slots[i].x;
        lv_coord_t w = slots[i].w;
        lv_coord_t cx = slots[i].cx;
        int t = slots[i].type;

        // 1. Kachel erstellen
        lv_obj_t *card = ui_create_card(parent, w, 74);
        lv_obj_set_pos(card, x, 0);
        lv_obj_set_style_pad_all(card, 6, 0);

        // 2. Fluss-Linie erstellen & symmetrisch an Batterie anbinden
        lv_obj_t **line_ptr = nullptr;
        lv_point_t *pts = nullptr;
        int pt_cnt = 0;

        if (t == 0) {
            line_ptr = &line_solar;
            pts = pts_solar;
        } else if (t == 1) {
            line_ptr = &line_shore;
            pts = pts_shore;
        } else {
            line_ptr = &line_alt;
            pts = pts_alt;
        }

        if (cx < 180) {
            lv_coord_t hub_x = (prod_count == 2) ? 160 : 150;
            pts[0] = {cx, 74};
            pts[1] = {cx, 94};
            pts[2] = {hub_x, 114};
            pt_cnt = 3;
        } else if (cx > 260) {
            lv_coord_t hub_x = (prod_count == 2) ? 280 : 290;
            pts[0] = {cx, 74};
            pts[1] = {cx, 94};
            pts[2] = {hub_x, 114};
            pt_cnt = 3;
        } else {
            pts[0] = {220, 74};
            pts[1] = {220, 114};
            pt_cnt = 2;
        }

        *line_ptr = lv_line_create(parent);
        lv_line_set_points(*line_ptr, pts, pt_cnt);
        lv_obj_set_style_line_width(*line_ptr, 3, 0);
        lv_obj_set_style_line_rounded(*line_ptr, true, 0);
        lv_obj_set_style_line_color(*line_ptr, ui_theme_border(), 0);

        // 3. Mini Arc Gauge (Rechts): Visuelle Auslastung relativ zur Maximalleistung
        lv_obj_t *arc = lv_arc_create(card);
        lv_obj_set_size(arc, 48, 48);
        lv_arc_set_rotation(arc, 135);
        lv_arc_set_bg_angles(arc, 0, 270);
        lv_arc_set_value(arc, 0);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(arc, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_arc_width(arc, 5, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, ui_theme_track(), LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 5, LV_PART_MAIN);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);

        lv_obj_t *lbl_pct = lv_label_create(arc);
        lv_obj_set_style_text_font(lbl_pct, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_pct, ui_theme_muted(), 0);
        lv_label_set_text(lbl_pct, "0%");
        lv_obj_align(lbl_pct, LV_ALIGN_CENTER, 0, 0);

        // 4. Inhalt der Kachel
        if (t == 0) {
            // Solar Card
            arc_solar = arc;
            lbl_solar_pct = lbl_pct;
            lv_obj_set_style_arc_color(arc_solar, lv_color_hex(UI_COLOR_WARNING), LV_PART_INDICATOR);

            lv_obj_t *l_sol_title = lv_label_create(card);
            lv_label_set_text_fmt(l_sol_title, "%s Solar", LV_SYMBOL_CHARGE);
            lv_obj_set_style_text_font(l_sol_title, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(l_sol_title, lv_color_hex(UI_COLOR_WARNING), 0);
            lv_obj_align(l_sol_title, LV_ALIGN_TOP_LEFT, 0, 0);

            lbl_solar_w = lv_label_create(card);
            lv_obj_set_style_text_font(lbl_solar_w, &lv_font_montserrat_18, 0);
            lv_obj_set_style_text_color(lbl_solar_w, lv_color_hex(UI_COLOR_WARNING), 0);
            lv_label_set_text(lbl_solar_w, "0 W");
            lv_obj_align(lbl_solar_w, LV_ALIGN_TOP_LEFT, 0, 16);

            lbl_solar_sub = lv_label_create(card);
            lv_obj_set_style_text_font(lbl_solar_sub, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_solar_sub, ui_theme_muted(), 0);
            lv_label_set_text(lbl_solar_sub, "--V  --A");
            lv_obj_align(lbl_solar_sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);

            // Klick auf das gesamte Solar-Feld öffnet automatisch den Solar-Verlauf
            ui_energy_history_button(card, true);
            lv_obj_t *sol_btn = lv_obj_get_child(card, -1);
            if (sol_btn) {
                lv_obj_set_size(sol_btn, LV_PCT(100), LV_PCT(100));
                lv_obj_set_pos(sol_btn, 0, 0);
                lv_obj_set_style_bg_opa(sol_btn, LV_OPA_0, 0);
                lv_obj_set_style_border_width(sol_btn, 0, 0);
                lv_obj_set_style_shadow_width(sol_btn, 0, 0);
                for (uint32_t c = 0; c < lv_obj_get_child_cnt(sol_btn); ++c) {
                    lv_obj_add_flag(lv_obj_get_child(sol_btn, c), LV_OBJ_FLAG_HIDDEN);
                }
            }
        } else if (t == 1) {
            // Landstrom Card
            arc_shore = arc;
            lbl_shore_pct = lbl_pct;
            lv_obj_set_style_arc_color(arc_shore, lv_color_hex(UI_COLOR_PRIMARY), LV_PART_INDICATOR);

            lv_obj_t *l_shore_title = lv_label_create(card);
            lv_label_set_text(l_shore_title, "Landstrom");
            lv_obj_set_style_text_font(l_shore_title, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(l_shore_title, lv_color_hex(UI_COLOR_PRIMARY), 0);
            lv_obj_align(l_shore_title, LV_ALIGN_TOP_LEFT, 0, 0);

            lbl_shore_w = lv_label_create(card);
            lv_obj_set_style_text_font(lbl_shore_w, &lv_font_montserrat_18, 0);
            lv_obj_set_style_text_color(lbl_shore_w, ui_theme_muted(), 0);
            lv_label_set_text(lbl_shore_w, "Off");
            lv_obj_align(lbl_shore_w, LV_ALIGN_TOP_LEFT, 0, 16);

            lbl_shore_sub = lv_label_create(card);
            lv_obj_set_style_text_font(lbl_shore_sub, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_shore_sub, ui_theme_muted(), 0);
            lv_label_set_text(lbl_shore_sub, "Getrennt");
            lv_obj_align(lbl_shore_sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        } else if (t == 2) {
            // Lichtmaschine Card
            arc_alt = arc;
            lbl_alt_pct = lbl_pct;
            lv_obj_set_style_arc_color(arc_alt, lv_color_hex(0xa855f7), LV_PART_INDICATOR);

            lv_obj_t *l_alt_title = lv_label_create(card);
            lv_label_set_text(l_alt_title, "Lichtmasch.");
            lv_obj_set_style_text_font(l_alt_title, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(l_alt_title, lv_color_hex(0xa855f7), 0);
            lv_obj_align(l_alt_title, LV_ALIGN_TOP_LEFT, 0, 0);

            lbl_alt_w = lv_label_create(card);
            lv_obj_set_style_text_font(lbl_alt_w, &lv_font_montserrat_18, 0);
            lv_obj_set_style_text_color(lbl_alt_w, ui_theme_muted(), 0);
            lv_label_set_text(lbl_alt_w, "Off");
            lv_obj_align(lbl_alt_w, LV_ALIGN_TOP_LEFT, 0, 16);

            lbl_alt_sub = lv_label_create(card);
            lv_obj_set_style_text_font(lbl_alt_sub, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_alt_sub, ui_theme_muted(), 0);
            lv_label_set_text(lbl_alt_sub, "Inaktiv");
            lv_obj_align(lbl_alt_sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        }
    }

    // =========================================================================
    // 2. TIER 2: ZENTRALE EINHEIT (Batterie - 132px Höhe, Arc mittig, % mittig, Spannung unter Watt/A)
    // =========================================================================
    card_bat_hub = ui_create_card(parent, 390, 132);
    lv_obj_align(card_bat_hub, LV_ALIGN_TOP_MID, 0, 114);
    lv_obj_set_style_pad_all(card_bat_hub, 10, 0);
    lv_obj_set_style_radius(card_bat_hub, 16, 0);

    // Links: Circular Arc Gauge - Vertikal mittig ausgerichtet, % Zahl genau mittig
    arc_battery = lv_arc_create(card_bat_hub);
    lv_obj_set_size(arc_battery, 106, 106);
    lv_arc_set_rotation(arc_battery, 135);
    lv_arc_set_bg_angles(arc_battery, 0, 270);
    lv_arc_set_value(arc_battery, 0);
    lv_obj_clear_flag(arc_battery, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(arc_battery, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_set_style_arc_color(arc_battery, lv_color_hex(UI_COLOR_SUCCESS), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_battery, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_battery, ui_theme_track(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_battery, 10, LV_PART_MAIN);
    lv_obj_remove_style(arc_battery, NULL, LV_PART_KNOB);

    lbl_bat_pct = lv_label_create(arc_battery);
    lv_obj_set_style_text_font(lbl_bat_pct, &lv_font_montserrat_28, 0);
    lv_label_set_text(lbl_bat_pct, "0%");
    lv_obj_align(lbl_bat_pct, LV_ALIGN_CENTER, 0, 0);

    // Rechts: "Batterie", Watt groß & Ampere nebeneinander, darunter Spannung
    lv_obj_t *box_bat_info = lv_obj_create(card_bat_hub);
    lv_obj_set_size(box_bat_info, 245, 110);
    lv_obj_align(box_bat_info, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(box_bat_info, LV_OPA_0, 0);
    lv_obj_set_style_border_width(box_bat_info, 0, 0);
    lv_obj_set_style_pad_all(box_bat_info, 2, 0);
    lv_obj_clear_flag(box_bat_info, LV_OBJ_FLAG_CLICKABLE);

    // Titel: Nur "Batterie"
    lv_obj_t *l_hub_title = lv_label_create(box_bat_info);
    lv_label_set_text_fmt(l_hub_title, "%s Batterie", LV_SYMBOL_BATTERY_3);
    lv_obj_set_style_text_font(l_hub_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l_hub_title, ui_theme_muted(), 0);
    lv_obj_align(l_hub_title, LV_ALIGN_TOP_LEFT, 0, 4);

    // Zeile: Watt groß und Ampere nebeneinander
    lv_obj_t *row_pwr = lv_obj_create(box_bat_info);
    lv_obj_set_size(row_pwr, 240, 40);
    lv_obj_align(row_pwr, LV_ALIGN_TOP_LEFT, 0, 26);
    lv_obj_set_style_bg_opa(row_pwr, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row_pwr, 0, 0);
    lv_obj_set_style_pad_all(row_pwr, 0, 0);
    lv_obj_set_flex_flow(row_pwr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_pwr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_pwr, 8, 0);
    lv_obj_clear_flag(row_pwr, LV_OBJ_FLAG_CLICKABLE);

    lbl_bat_watt = lv_label_create(row_pwr);
    lv_obj_set_style_text_font(lbl_bat_watt, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_bat_watt, lv_color_hex(UI_COLOR_SUCCESS), 0);
    lv_label_set_text(lbl_bat_watt, "0 W");

    lbl_bat_amp = lv_label_create(row_pwr);
    lv_obj_set_style_text_font(lbl_bat_amp, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_bat_amp, ui_theme_muted(), 0);
    lv_label_set_text(lbl_bat_amp, "(0.0 A)");

    // Unter Watt & Ampere: Batteriespannung
    lbl_bat_volt = lv_label_create(box_bat_info);
    lv_obj_set_style_text_font(lbl_bat_volt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_bat_volt, ui_theme_muted(), 0);
    lv_label_set_text(lbl_bat_volt, "-- V");
    lv_obj_align(lbl_bat_volt, LV_ALIGN_TOP_LEFT, 2, 70);

    // Klick auf das gesamte Batterie-Feld öffnet automatisch den Verlauf
    ui_energy_history_button(card_bat_hub, false);
    lv_obj_t *hist_btn = lv_obj_get_child(card_bat_hub, -1);
    if (hist_btn) {
        lv_obj_set_size(hist_btn, LV_PCT(100), LV_PCT(100));
        lv_obj_set_pos(hist_btn, 0, 0);
        lv_obj_set_style_bg_opa(hist_btn, LV_OPA_0, 0);
        lv_obj_set_style_border_width(hist_btn, 0, 0);
        lv_obj_set_style_shadow_width(hist_btn, 0, 0);
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(hist_btn); ++i) {
            lv_obj_add_flag(lv_obj_get_child(hist_btn, i), LV_OBJ_FLAG_HIDDEN);
        }
    }

    // =========================================================================
    // 3. FLUSS-LINIE (Zentrum -> Verbraucher: 40px Abstand)
    // =========================================================================
    line_load = lv_line_create(parent);
    lv_line_set_points(line_load, pts_load, 2);
    lv_obj_set_style_line_width(line_load, 3, 0);
    lv_obj_set_style_line_rounded(line_load, true, 0);
    lv_obj_set_style_line_color(line_load, ui_theme_border(), 0);

    // =========================================================================
    // 4. TIER 3: VERBRAUCHER (Unten: Höhe 74px wie Erzeuger, ganz unten & zentriert)
    // =========================================================================
    card_load = ui_create_card(parent, 360, 74);
    lv_obj_align(card_load, LV_ALIGN_TOP_MID, 0, 286);
    lv_obj_set_style_pad_all(card_load, 8, 0);
    lv_obj_set_style_radius(card_load, 16, 0);

    // Links: Icon & Titel
    lv_obj_t *l_load_icon = lv_label_create(card_load);
    lv_label_set_text(l_load_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(l_load_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l_load_icon, lv_color_hex(UI_COLOR_WARNING), 0);
    lv_obj_align(l_load_icon, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *l_load_title = lv_label_create(card_load);
    lv_label_set_text(l_load_title, "Verbraucher");
    lv_obj_set_style_text_font(l_load_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_load_title, ui_theme_text(), 0);
    lv_obj_align(l_load_title, LV_ALIGN_LEFT_MID, 36, 0);

    // Rechts: Nur Leistung (groß) & große Restzeit
    lbl_load_watt = lv_label_create(card_load);
    lv_obj_set_style_text_font(lbl_load_watt, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_load_watt, lv_color_hex(UI_COLOR_WARNING), 0);
    lv_label_set_text(lbl_load_watt, "0 W");
    lv_obj_align(lbl_load_watt, LV_ALIGN_TOP_RIGHT, -6, 2);

    lbl_load_sub = lv_label_create(card_load);
    lv_obj_set_style_text_font(lbl_load_sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_load_sub, ui_theme_muted(), 0);
    lv_label_set_text(lbl_load_sub, "Standby");
    lv_obj_align(lbl_load_sub, LV_ALIGN_BOTTOM_RIGHT, -6, -2);
}
