#include "ui_main.h"
#include "victron_settings.h"
#include "wifi_diagnostics.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#include "web_ota.h"
#include "upload_qr.h"

static lv_obj_t *sw_debug = NULL, *sw_demo = NULL;
static lv_obj_t *settings_tabview = nullptr;
static uint16_t victron_settings_index = 0;
static lv_obj_t * http_test_win = NULL;
static lv_obj_t * http_lbl_result = NULL;
static lv_obj_t *lbl_web_ota_url = NULL;
static lv_obj_t *lbl_web_ota_status = NULL;
static lv_obj_t *update_qr = NULL;
static lv_obj_t *card_web_ota = NULL;
static String update_qr_url;

static void close_http_test_cb(lv_event_t * e) {
    if (http_test_win) {
        lv_obj_del(http_test_win);
        http_test_win = NULL;
    }
}

static void run_http_fetch(lv_event_t * e) {
    if(!http_lbl_result) return;
    
    if(WiFi.status() != WL_CONNECTED) {
        lv_label_set_text(http_lbl_result, "WLAN nicht verbunden!");
        return;
    }
    
    lv_label_set_text(http_lbl_result, "Lade Daten von Port 1880...");
    lv_timer_handler(); // force redraw
    
    String baseUrl = "http://" + state.vanpi_ip + ":1880";
    String result = "";
    
    HTTPClient http;
    const char* endpoints[] = {"/batt", "/relay", "/wrelay", "/dimmer", "/names"};
    
    for(int i=0; i<5; i++) {
        String url = baseUrl + endpoints[i];
        http.begin(url);
        int code = http.GET();
        result += "=== " + String(endpoints[i]) + " ===\n";
        if(code > 0) {
            result += "Code " + String(code) + ":\n";
            result += http.getString() + "\n\n";
        } else {
            result += "Fehler: " + http.errorToString(code) + "\n\n";
        }
        http.end();
    }
    
    lv_label_set_text(http_lbl_result, result.c_str());
}

static void open_http_test_cb(lv_event_t * e) {
    http_test_win = lv_obj_create(lv_scr_act());
    lv_obj_set_size(http_test_win, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(http_test_win, state.dark_mode ? lv_color_hex(0x1e1e1e) : lv_color_hex(0xf0f0f0), 0);
    
    lv_obj_t * btn_close = lv_btn_create(http_test_win);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_t * lbl_c = lv_label_create(btn_close);
    lv_label_set_text(lbl_c, "X");
    lv_obj_add_event_cb(btn_close, close_http_test_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_run = lv_btn_create(http_test_win);
    lv_obj_align(btn_run, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t * lbl_r = lv_label_create(btn_run);
    lv_label_set_text(lbl_r, "Laden");
    lv_obj_add_event_cb(btn_run, run_http_fetch, LV_EVENT_CLICKED, NULL);
    
    http_lbl_result = lv_label_create(http_test_win);
    lv_obj_set_width(http_lbl_result, 440);
    lv_label_set_long_mode(http_lbl_result, LV_LABEL_LONG_WRAP);
    lv_obj_align(http_lbl_result, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_label_set_text(http_lbl_result, "Drücke Laden...");
}

#include <WiFi.h>
#include <Wire.h>
#include "WS_CH32_IO.h"
#include "ui_screensaver.h"

extern TwoWire Wire;

static lv_obj_t *dd_wifi_ssid;
static lv_obj_t *btn_wifi_scan;
static lv_obj_t *lbl_wifi_scan;
static lv_obj_t *ta_wifi_pass;
static lv_obj_t *password_eye_label, *wifi_status_label;
static lv_obj_t *ta_mqtt_ip;
static lv_obj_t *dd_out_temp = NULL;
static lv_obj_t *dd_in_temp = NULL;
static lv_obj_t *dd_cust_temp1 = NULL;
static lv_obj_t *dd_cust_temp2 = NULL;
static lv_obj_t *dd_fridge_temp = NULL;
static int temp_opt_map_primary[TEMP_SOURCE_COUNT];
static int temp_opt_count_primary = 0;
static int temp_opt_map_optional[TEMP_SOURCE_COUNT + 1];
static int temp_opt_count_optional = 0;



static lv_obj_t *kb;

static lv_obj_t *dd_rename_item;
static lv_obj_t *ta_rename;

static lv_obj_t *ta_bat_cap;
static lv_obj_t *ta_solar_max;
static lv_obj_t *ta_shore_max;
static lv_obj_t *ta_alt_max;
static lv_obj_t *ta_tank_max[4];
static lv_obj_t *lbl_tank_max[4];
static lv_obj_t *dd_tank_unit[4];
static lv_obj_t *dd_tank_count = NULL;
static lv_obj_t *sw_tank_liters = NULL;
static lv_obj_t *dd_pump = NULL;
static lv_obj_t *dd_drain = NULL;
static lv_obj_t *dd_heater_type = NULL;

static int relay_opt_map[32];
static int relay_opt_count = 0;

static void update_water_relay_dropdowns() {
    relay_opt_count = 0;
    String opts = "Nicht verfügbar\n";
    relay_opt_map[relay_opt_count++] = -1;

    int max_relays = state.show_relay_ext ? 16 : 8;
    for (int i = 0; i < max_relays; i++) {
        String nm = state.switch_names[i];
        if (nm.length() == 0 || nm == "CH " + String(i + 1)) {
            opts += "Relais " + String(i + 1);
        } else {
            opts += nm;
        }
        opts += "\n";
        relay_opt_map[relay_opt_count++] = i;
    }

    if (state.show_wrelay) {
        for (int i = 0; i < 8; i++) {
            String nm = state.wrelay_names[i];
            if (nm.length() == 0 || nm == "wrelay" + String(i + 1)) {
                opts += "WLAN " + String(i + 1);
            } else {
                opts += nm + " (WLAN)";
            }
            if (i != 7) opts += "\n";
            relay_opt_map[relay_opt_count++] = 16 + i;
        }
    } else {
        if (opts.endsWith("\n")) opts.remove(opts.length() - 1);
    }

    if (dd_pump) {
        lv_dropdown_set_options(dd_pump, opts.c_str());
        int sel = 0;
        for (int k = 0; k < relay_opt_count; k++) {
            if (relay_opt_map[k] == state.pump_relay) { sel = k; break; }
        }
        lv_dropdown_set_selected(dd_pump, sel);
    }

    if (dd_drain) {
        lv_dropdown_set_options(dd_drain, opts.c_str());
        int sel = 0;
        for (int k = 0; k < relay_opt_count; k++) {
            if (relay_opt_map[k] == state.drain_relay) { sel = k; break; }
        }
        lv_dropdown_set_selected(dd_drain, sel);
    }
}

static void update_tank_max_visibility() {
    int cnt = constrain(state.tank_count, 1, 4);
    for (int i = 0; i < 4; i++) {
        if (lbl_tank_max[i] && ta_tank_max[i]) {
            if (i < cnt) {
                lv_obj_clear_flag(lbl_tank_max[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ta_tank_max[i], LV_OBJ_FLAG_HIDDEN);
                if (dd_tank_unit[i]) lv_obj_clear_flag(dd_tank_unit[i], LV_OBJ_FLAG_HIDDEN);
                String name = state.tank_names[i];
                if (name.length() == 0) name = "Tank " + String(i + 1);
                if (name.length() > 9) name = name.substring(0, 8) + ".";
                lv_label_set_text_fmt(lbl_tank_max[i], "T%d (%s):", i + 1, name.c_str());
            } else {
                lv_obj_add_flag(lbl_tank_max[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ta_tank_max[i], LV_OBJ_FLAG_HIDDEN);
                if (dd_tank_unit[i]) lv_obj_add_flag(dd_tank_unit[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static lv_obj_t *ta_warn_frost;
static lv_obj_t *ta_warn_bat;
static lv_obj_t *ta_warn_bat_yellow;
static lv_obj_t *ta_warn_fresh;
static lv_obj_t *ta_warn_waste;

lv_obj_t *lbl_debug_info = NULL;

static bool scan_in_progress = false;

static lv_obj_t *card_shore_toggle = nullptr;
static lv_obj_t *sw_shore_toggle = nullptr;
static lv_obj_t *lbl_shore_toggle = nullptr;

static lv_obj_t *card_alt_toggle = nullptr;
static lv_obj_t *sw_alt_toggle = nullptr;
static lv_obj_t *lbl_alt_toggle = nullptr;

static void ui_settings_update_producers() {
    if (card_shore_toggle && sw_shore_toggle && lv_obj_is_valid(card_shore_toggle)) {
        if (!state.shore_available) {
            lv_obj_clear_state(sw_shore_toggle, LV_STATE_CHECKED);
            lv_obj_add_state(sw_shore_toggle, LV_STATE_DISABLED);
            ui_border_color_if_changed(card_shore_toggle, ui_theme_track());
            lv_obj_clear_flag(card_shore_toggle, LV_OBJ_FLAG_CLICKABLE);
            if (lbl_shore_toggle && lv_obj_is_valid(lbl_shore_toggle)) {
                ui_text_color_if_changed(lbl_shore_toggle, ui_theme_muted());
            }
        } else {
            lv_obj_clear_state(sw_shore_toggle, LV_STATE_DISABLED);
            if (state.power_show_shore) lv_obj_add_state(sw_shore_toggle, LV_STATE_CHECKED);
            else lv_obj_clear_state(sw_shore_toggle, LV_STATE_CHECKED);
            ui_border_color_if_changed(card_shore_toggle, ui_theme_border());
            lv_obj_add_flag(card_shore_toggle, LV_OBJ_FLAG_CLICKABLE);
            if (lbl_shore_toggle && lv_obj_is_valid(lbl_shore_toggle)) {
                ui_text_color_if_changed(lbl_shore_toggle, ui_theme_text());
            }
        }
    }
    if (card_alt_toggle && sw_alt_toggle && lv_obj_is_valid(card_alt_toggle)) {
        if (!state.alt_available) {
            lv_obj_clear_state(sw_alt_toggle, LV_STATE_CHECKED);
            lv_obj_add_state(sw_alt_toggle, LV_STATE_DISABLED);
            ui_border_color_if_changed(card_alt_toggle, ui_theme_track());
            lv_obj_clear_flag(card_alt_toggle, LV_OBJ_FLAG_CLICKABLE);
            if (lbl_alt_toggle && lv_obj_is_valid(lbl_alt_toggle)) {
                ui_text_color_if_changed(lbl_alt_toggle, ui_theme_muted());
            }
        } else {
            lv_obj_clear_state(sw_alt_toggle, LV_STATE_DISABLED);
            if (state.power_show_alt) lv_obj_add_state(sw_alt_toggle, LV_STATE_CHECKED);
            else lv_obj_clear_state(sw_alt_toggle, LV_STATE_CHECKED);
            ui_border_color_if_changed(card_alt_toggle, ui_theme_border());
            lv_obj_add_flag(card_alt_toggle, LV_OBJ_FLAG_CLICKABLE);
            if (lbl_alt_toggle && lv_obj_is_valid(lbl_alt_toggle)) {
                ui_text_color_if_changed(lbl_alt_toggle, ui_theme_text());
            }
        }
    }
}

void ui_sync_demo_controls() {
    lv_obj_t *controls[] = {sw_debug, sw_demo};
    for (lv_obj_t *control : controls) {
        if (!control || !lv_obj_is_valid(control)) continue;
        bool checked = lv_obj_has_state(control, LV_STATE_CHECKED);
        if (state.debug_mode && !checked) lv_obj_add_state(control, LV_STATE_CHECKED);
        else if (!state.debug_mode && checked) lv_obj_clear_state(control, LV_STATE_CHECKED);
    }
}
void ui_update_settings_tab() {
    ui_sync_demo_controls();
    if (lv_scr_act() == scr_settings) {
        ui_victron_settings_update();
        ui_settings_update_producers();
    }
    if (lv_scr_act() == scr_settings && wifi_status_label) {
        String details = wifi_connection_details();
        if (strcmp(lv_label_get_text(wifi_status_label), details.c_str()) != 0) {
            lv_label_set_text(wifi_status_label, details.c_str());
            if (card_web_ota) {
                lv_obj_update_layout(wifi_status_label);
                lv_obj_set_y(card_web_ota, lv_obj_get_y(wifi_status_label) +
                    lv_obj_get_height(wifi_status_label) + 16);
            }
        }
    }
    if (lv_scr_act() == scr_settings && lbl_web_ota_url) {
        String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "...";
        char buf[128];
        snprintf(buf, sizeof(buf), "Adresse: http://%s/settings", ip.c_str());
        ui_label_set_text_if_changed(lbl_web_ota_url, buf);

        String url = WiFi.status() == WL_CONNECTED ? "http://" + ip + "/settings" : "";
        if (update_qr && update_qr_url != url) {
            update_qr_url = url;
            if (url.length() && upload_qr_update(update_qr, url.c_str()))
                lv_obj_clear_flag(update_qr, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(update_qr, LV_OBJ_FLAG_HIDDEN);
        }
        if (state.web_ota_active) {
            if (lbl_web_ota_status) {
                ui_label_set_text_if_changed(lbl_web_ota_status, state.web_ota_msg.c_str());
            }
        } else {
            if (lbl_web_ota_status) {
                char st_buf[128];
                snprintf(st_buf, sizeof(st_buf), "%s\n%s", CAMPERUI_VERSION, WiFi.status() == WL_CONNECTED ? "Handy im selben WLAN: QR scannen fuer Einstellungen." : "WLAN verbinden, um den QR-Code anzuzeigen.");
                ui_label_set_text_if_changed(lbl_web_ota_status, st_buf);
            }
        }
    }
    if (ui_home_settings_is_active()) ui_home_settings_refresh();
    if (scan_in_progress) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            scan_in_progress = false;
            // Keep the saved WLAN selectable even when it is out of range.
            String options = state.wifi_ssid;
            for (int i = 0; i < n; ++i) {
                String ssid = WiFi.SSID(i);
                if (ssid.length() == 0 || ssid == state.wifi_ssid) continue;
                if (options.length() > 0) options += "\n";
                options += ssid;
            }
            if (options.length() == 0) options = "Keine Netzwerke gefunden";
            if (dd_wifi_ssid) lv_dropdown_set_options(dd_wifi_ssid, options.c_str());
            if (lbl_wifi_scan) lv_label_set_text(lbl_wifi_scan, "Scan");
            WiFi.scanDelete();
            wifi_finish_scan();
        } else if (n == WIFI_SCAN_FAILED) {
            scan_in_progress = false;
            WiFi.scanDelete();
            if (lbl_wifi_scan) lv_label_set_text(lbl_wifi_scan, "Scan");
            wifi_finish_scan();
        }
    }
}

static void ta_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(kb); // Bring keyboard to front
    }
    else if(code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
            lv_indev_reset(NULL, ta); // clear focus
        }
    }
}

static void kb_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_indev_reset(NULL, NULL);
    }
}

static void wifi_scan_cb(lv_event_t * e) {
    if(!scan_in_progress) {
        wifi_prepare_for_scan();
        if (lbl_wifi_scan) lv_label_set_text(lbl_wifi_scan, "Scanne...");
        int16_t res = WiFi.scanNetworks(true); // async
        if (res == WIFI_SCAN_FAILED) {
            WiFi.disconnect(true, false);
            delay(100);
            WiFi.scanNetworks(true);
        }
        scan_in_progress = true;
    }
}

static void save_settings_cb(lv_event_t * e) {
    String old_ssid, old_pass;
    {
        StateLockGuard lock;
        old_ssid = state.wifi_ssid;
        old_pass = state.wifi_pass;
    }
    String new_ssid = old_ssid;
    String new_pass = old_pass;
    if (dd_wifi_ssid && lv_dropdown_get_option_cnt(dd_wifi_ssid) > 0) {
        char buf[64] = {0};
        lv_dropdown_get_selected_str(dd_wifi_ssid, buf, sizeof(buf));
        new_ssid = String(buf);
    }
    if (ta_wifi_pass) {
        new_pass = String(lv_textarea_get_text(ta_wifi_pass));
    }
    bool wifi_changed = (new_ssid != old_ssid) || (new_pass != old_pass);
    {
        StateLockGuard lock;
        state.wifi_ssid = new_ssid;
        state.wifi_pass = new_pass;
        if (ta_mqtt_ip) {
            state.vanpi_ip = String(lv_textarea_get_text(ta_mqtt_ip));
        }
    }
    
    state_save();
    
    // Only reconnect WiFi if credentials changed or WiFi is not currently connected
    if (wifi_changed || (new_ssid.length() > 0 && WiFi.status() != WL_CONNECTED)) {
        WiFi.disconnect();
        wifi_connect_configured();
    }
    
    // Load main screen cleanly
    if (scr_main) lv_scr_load(scr_main);
    // Visibility is applied when cards are built; rebuild after the event returns.
    ui_init();
}

static String build_temp_dropdown_options(bool include_none, int *map_arr, int &count_out) {
    String opts = "";
    count_out = 0;
    if (include_none) {
        opts += "Nicht anzeigen";
        map_arr[count_out++] = -1;
    }
    for (int i = 0; i < TEMP_SOURCE_COUNT; ++i) {
        if (opts.length() > 0) opts += "\n";
        String label = (i < 4 ? "Temp " + String(i + 1) : "Ruuvi " + String(i - 4));
        if (state.temp_sensor_names[i].length() > 0 && 
            state.temp_sensor_names[i] != label) {
            label += " (" + state.temp_sensor_names[i] + ")";
        }
        if (!(state.temp_fields & (1 << i))) {
            label += " [offline]";
        }
        opts += label;
        map_arr[count_out++] = i;
    }
    return opts;
}

static void update_all_temp_dropdown_options() {
    String opts_prim = build_temp_dropdown_options(false, temp_opt_map_primary, temp_opt_count_primary);
    String opts_opt = build_temp_dropdown_options(true, temp_opt_map_optional, temp_opt_count_optional);

    auto select_in_map = [](lv_obj_t *dd, int *map_arr, int count, int current_val) {
        if (!dd) return;
        int sel = 0;
        for (int i = 0; i < count; ++i) {
            if (map_arr[i] == current_val) {
                sel = i;
                break;
            }
        }
        lv_dropdown_set_selected(dd, sel);
    };

    if (dd_out_temp) {
        lv_dropdown_set_options(dd_out_temp, opts_prim.c_str());
        select_in_map(dd_out_temp, temp_opt_map_primary, temp_opt_count_primary, state.outdoor_temp_sensor);
    }
    if (dd_in_temp) {
        lv_dropdown_set_options(dd_in_temp, opts_prim.c_str());
        select_in_map(dd_in_temp, temp_opt_map_primary, temp_opt_count_primary, state.indoor_temp_sensor);
    }
    if (dd_cust_temp1) {
        lv_dropdown_set_options(dd_cust_temp1, opts_opt.c_str());
        select_in_map(dd_cust_temp1, temp_opt_map_optional, temp_opt_count_optional, state.temps_custom_sensor[0]);
    }
    if (dd_cust_temp2) {
        lv_dropdown_set_options(dd_cust_temp2, opts_opt.c_str());
        select_in_map(dd_cust_temp2, temp_opt_map_optional, temp_opt_count_optional, state.temps_custom_sensor[1]);
    }
    if (dd_fridge_temp) {
        lv_dropdown_set_options(dd_fridge_temp, opts_opt.c_str());
        select_in_map(dd_fridge_temp, temp_opt_map_optional, temp_opt_count_optional, state.fridge_temp_sensor);
    }
}

static void open_settings_cb(lv_event_t * e) {
    update_all_temp_dropdown_options();
    lv_textarea_set_password_mode(ta_wifi_pass, true);
    lv_label_set_text(password_eye_label, LV_SYMBOL_EYE_OPEN);
    lv_scr_load(scr_settings);
}

void ui_open_settings() {
    open_settings_cb(nullptr);
}

void ui_open_victron_settings() {
    if (!scr_settings || !settings_tabview) return;
    lv_tabview_set_act(settings_tabview, victron_settings_index, LV_ANIM_OFF);
    open_settings_cb(nullptr);
}

// ==========================================
// 5th Tab on Main Screen
// ==========================================
void ui_build_settings(lv_obj_t *parent) {
    ui_setup_tab_page(parent);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 12, 0);
    
    // Modern Action Button
    lv_obj_t *btn_open = lv_btn_create(parent);
    lv_obj_set_size(btn_open, 320, 60);
    lv_obj_set_style_radius(btn_open, ui_radius_button(), 0);
    lv_obj_set_style_bg_color(btn_open, ui_theme_primary(), 0);
    lv_obj_set_style_shadow_width(btn_open, 0, 0);
    lv_obj_add_event_cb(btn_open, open_settings_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_open = lv_label_create(btn_open);
    lv_label_set_text_fmt(lbl_open, "%s System-Einstellungen", LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(lbl_open, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_open, ui_theme_btn_active_text(), 0);
    lv_obj_center(lbl_open);
    
    lv_obj_t *btn_home = lv_btn_create(parent);
    lv_obj_set_size(btn_home, 320, 46);
    lv_obj_set_style_radius(btn_home, ui_radius_button(), 0);
    lv_obj_set_style_bg_color(btn_home, ui_theme_primary(), 0);
    lv_obj_add_event_cb(btn_home, [](lv_event_t *) { ui_open_home_settings(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, LV_SYMBOL_HOME " Home-Einstellungen");
    lv_obj_set_style_text_font(lbl_home, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_home, ui_theme_btn_active_text(), 0);
    lv_obj_center(lbl_home);

    lv_obj_t *btn_nav = lv_btn_create(parent);
    lv_obj_set_size(btn_nav, 320, 46);
    lv_obj_set_style_radius(btn_nav, ui_radius_button(), 0);
    lv_obj_set_style_bg_color(btn_nav, ui_theme_primary(), 0);
    lv_obj_add_event_cb(btn_nav, [](lv_event_t *) { ui_open_nav_settings(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_nav = lv_label_create(btn_nav);
    lv_label_set_text(lbl_nav, LV_SYMBOL_LIST "  Menüleiste / Tabs anpassen");
    lv_obj_set_style_text_font(lbl_nav, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_nav, ui_theme_btn_active_text(), 0);
    lv_obj_center(lbl_nav);

    // Simulation / Debug Toggle Card
    lv_obj_t *card_dbg = ui_create_card(parent, 360, 56);
    lv_obj_set_style_pad_all(card_dbg, 10, 0);
    
    sw_debug = lv_switch_create(card_dbg);
    if (state.debug_mode) lv_obj_add_state(sw_debug, LV_STATE_CHECKED);
    lv_obj_align(sw_debug, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(sw_debug, [](lv_event_t * e) {
        lv_obj_t *sw = lv_event_get_target(e);
        state.debug_mode = lv_obj_has_state(sw, LV_STATE_CHECKED);
        ui_sync_demo_controls();
        ui_update_data();
        state_save();
    }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t *lbl_sw_dbg = lv_label_create(card_dbg);
    lv_label_set_text(lbl_sw_dbg, "Simulation / Dummy-Daten");
    lv_obj_set_style_text_font(lbl_sw_dbg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_sw_dbg, ui_theme_text(), 0);
    lv_obj_align(lbl_sw_dbg, LV_ALIGN_LEFT_MID, 65, 0);

    // Status Card
    lv_obj_t *card_status = ui_create_card(parent, 360, 110);
    lv_obj_set_style_pad_all(card_status, 12, 0);
    
    lbl_debug_info = lv_label_create(card_status);
    lv_label_set_text_fmt(lbl_debug_info, "%s%s\nInitialisiere...", state.debug_mode ? "Demo " : "", CAMPERUI_VERSION);
    lv_obj_set_style_text_font(lbl_debug_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(lbl_debug_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_debug_info, ui_theme_text(), 0);
    lv_obj_center(lbl_debug_info);
}

// ==========================================
// The Dedicated Settings Screen
// ==========================================
void ui_settings_reset() {
    settings_tabview = nullptr;
    victron_settings_index = 0;
    ui_victron_settings_reset();
    dd_pump = nullptr;
    dd_drain = nullptr;
    dd_tank_count = nullptr;
    sw_tank_liters = nullptr;
    for (int i = 0; i < 4; i++) {
        ta_tank_max[i] = nullptr;
        lbl_tank_max[i] = nullptr;
        dd_tank_unit[i] = nullptr;
    }
    dd_heater_type = nullptr;
    dd_wifi_ssid = nullptr;
    btn_wifi_scan = nullptr;
    lbl_wifi_scan = nullptr;
    ta_wifi_pass = nullptr;
    password_eye_label = nullptr;
    wifi_status_label = nullptr;
    ta_mqtt_ip = nullptr;
    dd_out_temp = nullptr;
    dd_in_temp = nullptr;
    dd_cust_temp1 = nullptr;
    dd_cust_temp2 = nullptr;
    dd_fridge_temp = nullptr;
    kb = nullptr;
    dd_rename_item = nullptr;
    ta_rename = nullptr;
    ta_bat_cap = nullptr;
    ta_solar_max = nullptr;
    ta_shore_max = nullptr;
    ta_alt_max = nullptr;
    sw_debug = nullptr;
    sw_demo = nullptr;
    http_test_win = nullptr;
    http_lbl_result = nullptr;
    lbl_web_ota_url = nullptr;
    lbl_web_ota_status = nullptr;
    update_qr = nullptr;
    card_web_ota = nullptr;
    ta_warn_frost = nullptr;
    ta_warn_bat = nullptr;
    ta_warn_bat_yellow = nullptr;
    ta_warn_fresh = nullptr;
    ta_warn_waste = nullptr;
}

void ui_settings_screen_init() {
    ui_settings_reset();
    scr_settings = lv_obj_create(NULL);
    lv_obj_clear_flag(scr_settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr_settings, ui_theme_bg(), 0);
    lv_obj_set_style_text_font(scr_settings, &lv_font_montserrat_14, 0);
    
    // Create Top Tabview
    // Create Top Tabview (54px header for comfortable touch targets and icons)
    lv_obj_t *tv = lv_tabview_create(scr_settings, LV_DIR_TOP, 54);
    settings_tabview = tv;
    lv_obj_set_size(tv, 480, 416); // Leaves space at bottom for the Save button
    lv_obj_align(tv, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(tv, LV_OBJ_FLAG_SCROLLABLE);
    if (lv_tabview_get_content(tv)) {
        lv_obj_clear_flag(lv_tabview_get_content(tv), LV_OBJ_FLAG_SCROLLABLE);
    }
    
    lv_obj_t *t_net  = lv_tabview_add_tab(tv, MDI_WIFI);
    lv_obj_t *t_gen  = lv_tabview_add_tab(tv, MDI_COG);
    lv_obj_t *t_vis  = lv_tabview_add_tab(tv, MDI_EYE);
    lv_obj_t *t_val  = lv_tabview_add_tab(tv, MDI_SLIDERS);
    lv_obj_t *t_alm  = lv_tabview_add_tab(tv, MDI_BELL);
    victron_settings_index = lv_obj_get_child_cnt(lv_tabview_get_content(tv));
    lv_obj_t *t_gx = lv_tabview_add_tab(tv, "VE");
    ui_victron_settings_build(t_gx, ta_event_cb);

    lv_obj_t *scroll_tabs[] = {t_net, t_vis, t_val, t_alm, t_gx};
    for (lv_obj_t *st : scroll_tabs) {
        if (!st) continue;
        lv_obj_add_flag(st, LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_clear_flag(st, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    }

    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tv);
    if (tab_btns) {
        lv_obj_set_style_pad_left(tab_btns, 0, 0);
        lv_obj_set_style_pad_right(tab_btns, 0, 0);
        lv_obj_set_style_text_font(tab_btns, &ui_font_mdi_24, 0);
        lv_obj_add_event_cb(tab_btns, [](lv_event_t *event) {
            lv_obj_draw_part_dsc_t *part=lv_event_get_draw_part_dsc(event);
            if(part && part->part==LV_PART_ITEMS && part->label_dsc)
                part->label_dsc->font=part->id==victron_settings_index?&lv_font_montserrat_24:&ui_font_mdi_24;
        }, LV_EVENT_DRAW_PART_BEGIN, nullptr);
    }
    
    // ==========================================
    // TAB 1: NETZWERK
    // ==========================================
    lv_obj_add_flag(t_net, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(t_net, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(t_net, 80, 0);
    
    // WLAN Section Header
    lv_obj_t *l_w = lv_label_create(t_net);
    lv_label_set_text(l_w, "WLAN Verbindung");
    lv_obj_set_style_text_font(l_w, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_w, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_w, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Row 1: SSID Dropdown (w=305, h=44) + Scan Button (w=125, h=44)
    dd_wifi_ssid = lv_dropdown_create(t_net);
    lv_dropdown_set_options(dd_wifi_ssid, state.wifi_ssid.c_str());
    lv_obj_set_size(dd_wifi_ssid, 305, 44);
    lv_obj_set_style_text_font(dd_wifi_ssid, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_wifi_ssid, LV_ALIGN_TOP_LEFT, 0, 26);
    
    btn_wifi_scan = lv_btn_create(t_net);
    lv_obj_set_size(btn_wifi_scan, 125, 44);
    lv_obj_align(btn_wifi_scan, LV_ALIGN_TOP_LEFT, 315, 26);
    lv_obj_set_style_radius(btn_wifi_scan, ui_radius_button(), 0);
    lv_obj_set_style_bg_color(btn_wifi_scan, ui_theme_primary(), 0);
    lv_obj_add_event_cb(btn_wifi_scan, wifi_scan_cb, LV_EVENT_CLICKED, NULL);
    lbl_wifi_scan = lv_label_create(btn_wifi_scan);
    lv_label_set_text(lbl_wifi_scan, LV_SYMBOL_REFRESH " Scan");
    lv_obj_set_style_text_font(lbl_wifi_scan, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_wifi_scan);
    
    // Row 2: Password Textarea (w=365, h=44) + Eye Toggle (w=65, h=44)
    ta_wifi_pass = lv_textarea_create(t_net);
    lv_textarea_set_password_mode(ta_wifi_pass, true);
    lv_textarea_set_one_line(ta_wifi_pass, true);
    lv_textarea_set_text(ta_wifi_pass, state.wifi_pass.c_str());
    lv_textarea_set_placeholder_text(ta_wifi_pass, "WLAN Passwort");
    lv_obj_set_size(ta_wifi_pass, 365, 44);
    lv_obj_set_style_text_font(ta_wifi_pass, &lv_font_montserrat_14, 0);
    lv_obj_align(ta_wifi_pass, LV_ALIGN_TOP_LEFT, 0, 80);
    lv_obj_add_event_cb(ta_wifi_pass, ta_event_cb, LV_EVENT_ALL, NULL);
    
    lv_obj_t *password_eye = lv_btn_create(t_net);
    lv_obj_set_size(password_eye, 65, 44);
    lv_obj_align(password_eye, LV_ALIGN_TOP_LEFT, 375, 80);
    password_eye_label = lv_label_create(password_eye);
    lv_label_set_text(password_eye_label, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_font(password_eye_label, &lv_font_montserrat_16, 0);
    lv_obj_center(password_eye_label);
    lv_obj_add_event_cb(password_eye, [](lv_event_t *) {
        bool show = lv_textarea_get_password_mode(ta_wifi_pass);
        lv_textarea_set_password_mode(ta_wifi_pass, !show);
        lv_label_set_text(password_eye_label, show ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    }, LV_EVENT_CLICKED, nullptr);

    // Row 3: VanPi Server IP
    lv_obj_t *l_m = lv_label_create(t_net);
    lv_label_set_text(l_m, "VanPi Server IP");
    lv_obj_set_style_text_font(l_m, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_m, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_m, LV_ALIGN_TOP_LEFT, 0, 136);
    
    ta_mqtt_ip = lv_textarea_create(t_net);
    lv_textarea_set_one_line(ta_mqtt_ip, true);
    lv_textarea_set_text(ta_mqtt_ip, state.vanpi_ip.c_str());
    lv_textarea_set_placeholder_text(ta_mqtt_ip, "IP-Adresse (z.B. 192.168.1.100)");
    lv_obj_set_size(ta_mqtt_ip, 440, 44);
    lv_obj_set_style_text_font(ta_mqtt_ip, &lv_font_montserrat_14, 0);
    lv_obj_align(ta_mqtt_ip, LV_ALIGN_TOP_LEFT, 0, 162);
    lv_obj_add_event_cb(ta_mqtt_ip, ta_event_cb, LV_EVENT_ALL, NULL);
    
    // Row 4: WiFi Status Info
    wifi_status_label = lv_label_create(t_net);
    lv_obj_set_pos(wifi_status_label, 0, 218);
    lv_obj_set_width(wifi_status_label, 440);
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(wifi_status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(wifi_status_label, wifi_connection_details().c_str());

    // Web-Update Card
    card_web_ota = ui_create_card(t_net, 440, 330);
    lv_obj_update_layout(wifi_status_label);
    lv_obj_set_pos(card_web_ota, 0, lv_obj_get_y(wifi_status_label) +
        lv_obj_get_height(wifi_status_label) + 16);
    lv_obj_set_style_pad_all(card_web_ota, 12, 0);
    update_qr_url = "";
    // Local monochrome canvas; independent of the installed LVGL QR configuration.
    update_qr = upload_qr_create(card_web_ota);
    if (update_qr) {
        lv_obj_set_pos(update_qr, 118, 116);

        lv_obj_set_style_pad_all(update_qr, 0, 0);
        lv_obj_add_flag(update_qr, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *l_web_head = lv_label_create(card_web_ota);
    lv_label_set_text(l_web_head, LV_SYMBOL_SETTINGS " Web-Interface & Einstellungen");
    lv_obj_set_style_text_font(l_web_head, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_web_head, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_web_head, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_web_ota_url = lv_label_create(card_web_ota);
    lv_obj_set_width(lbl_web_ota_url, 416);
    lv_obj_set_style_text_font(lbl_web_ota_url, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_web_ota_url, lv_color_hex(UI_COLOR_SUCCESS), 0);
    lv_obj_align(lbl_web_ota_url, LV_ALIGN_TOP_LEFT, 0, 26);
    lv_label_set_text(lbl_web_ota_url, "Browser-Adresse: http://.../settings");

    lbl_web_ota_status = lv_label_create(card_web_ota);
    lv_obj_set_width(lbl_web_ota_status, 416);
    lv_label_set_long_mode(lbl_web_ota_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_web_ota_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_web_ota_status, ui_theme_muted(), 0);
    lv_obj_align(lbl_web_ota_status, LV_ALIGN_TOP_LEFT, 0, 52);
    lv_label_set_text_fmt(lbl_web_ota_status, "%s\n%s", CAMPERUI_VERSION, WiFi.status() == WL_CONNECTED ? "Handy im selben WLAN: QR scannen fuer Einstellungen." : "WLAN verbinden, um den QR-Code anzuzeigen.");

    // ==========================================
    // TAB 2: ALLGEMEIN
    // ==========================================
    lv_obj_clear_flag(t_gen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Left Column: Display Settings
    lv_obj_t *l_disp = lv_label_create(t_gen);
    lv_label_set_text(l_disp, "Display & System");
    lv_obj_set_style_text_font(l_disp, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_disp, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_disp, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lv_obj_t *l_bri = lv_label_create(t_gen);
    lv_label_set_text(l_bri, "Helligkeit:");
    lv_obj_set_style_text_font(l_bri, &lv_font_montserrat_14, 0);
    lv_obj_align(l_bri, LV_ALIGN_TOP_LEFT, 0, 30);

    lv_obj_t *sl_bri = lv_slider_create(t_gen);
    lv_slider_set_range(sl_bri, 10, 100);
    lv_slider_set_value(sl_bri, state.display_brightness, LV_ANIM_OFF);
    lv_obj_set_size(sl_bri, 125, 20);
    lv_obj_align(sl_bri, LV_ALIGN_TOP_LEFT, 85, 30);
    lv_obj_clear_flag(sl_bri, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(sl_bri, [](lv_event_t * e) {
        static uint8_t last_pwm = 255;
        lv_obj_t *slider = lv_event_get_target(e);
        state.display_brightness = lv_slider_get_value(slider);
        uint8_t pwm = ui_screensaver_brightness_to_pwm(state.display_brightness);
        if (pwm != last_pwm) {
            last_pwm = pwm;
            WS_CH32_IO::setPwm(Wire, pwm);
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sl_bri, [](lv_event_t * e) {
        state_save();
    }, LV_EVENT_RELEASED, NULL);
    
    // Dunkler Modus Row
    lv_obj_t *sw_theme = lv_switch_create(t_gen);
    lv_obj_set_size(sw_theme, 48, 26);
    if(state.dark_mode) lv_obj_add_state(sw_theme, LV_STATE_CHECKED);
    lv_obj_align(sw_theme, LV_ALIGN_TOP_LEFT, 0, 68);
    lv_obj_add_event_cb(sw_theme, [](lv_event_t * e) {
        lv_obj_t *sw = lv_event_get_target(e);
        state.dark_mode = lv_obj_has_state(sw, LV_STATE_CHECKED);
        state_save();
        ui_init(); // Reload theme
    }, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *l_theme = lv_label_create(t_gen);
    lv_label_set_text(l_theme, "Dunkler Modus");
    lv_obj_set_style_text_font(l_theme, &lv_font_montserrat_14, 0);
    lv_obj_align(l_theme, LV_ALIGN_TOP_LEFT, 58, 72);
    
    // Display Timeout
    lv_obj_t *l_time = lv_label_create(t_gen);
    lv_label_set_text(l_time, "Timeout:");
    lv_obj_set_style_text_font(l_time, &lv_font_montserrat_14, 0);
    lv_obj_align(l_time, LV_ALIGN_TOP_LEFT, 0, 116);

    lv_obj_t *dd_time = lv_dropdown_create(t_gen);
    lv_dropdown_set_options(dd_time, "15 Sekunden\n30 Sekunden\n1 Minute\n5 Minuten\nImmer an");
    if(state.display_timeout == 15) lv_dropdown_set_selected(dd_time, 0);
    else if(state.display_timeout == 30) lv_dropdown_set_selected(dd_time, 1);
    else if(state.display_timeout == 60) lv_dropdown_set_selected(dd_time, 2);
    else if(state.display_timeout == 300) lv_dropdown_set_selected(dd_time, 3);
    else lv_dropdown_set_selected(dd_time, 4);
    lv_obj_set_size(dd_time, 140, 44);
    lv_obj_set_style_text_font(dd_time, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_time, LV_ALIGN_TOP_LEFT, 68, 106);
    lv_obj_add_event_cb(dd_time, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        int sel = lv_dropdown_get_selected(dd);
        if (sel == 0) state.display_timeout = 15;
        else if (sel == 1) state.display_timeout = 30;
        else if (sel == 2) state.display_timeout = 60;
        else if (sel == 3) state.display_timeout = 300;
        else if (sel == 4) state.display_timeout = 0;
        state_save();
    }, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Demo Mode Switch
    sw_demo = lv_switch_create(t_gen);
    lv_obj_set_size(sw_demo, 48, 26);
    if(state.debug_mode) lv_obj_add_state(sw_demo, LV_STATE_CHECKED);
    lv_obj_align(sw_demo, LV_ALIGN_TOP_LEFT, 0, 164);
    lv_obj_add_event_cb(sw_demo, [](lv_event_t * e) {
        lv_obj_t *sw = lv_event_get_target(e);
        state.debug_mode = lv_obj_has_state(sw, LV_STATE_CHECKED);
        ui_sync_demo_controls();
        state_save();
    }, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *l_demo = lv_label_create(t_gen);
    lv_label_set_text(l_demo, "Demo / Simulation");
    lv_obj_set_style_text_font(l_demo, &lv_font_montserrat_14, 0);
    lv_obj_align(l_demo, LV_ALIGN_TOP_LEFT, 58, 168);

    // Reboot Button
    lv_obj_t *btn_reboot = lv_btn_create(t_gen);
    lv_obj_set_size(btn_reboot, 205, 46);
    lv_obj_align(btn_reboot, LV_ALIGN_TOP_LEFT, 0, 212);
    lv_obj_set_style_bg_color(btn_reboot, lv_color_hex(UI_COLOR_DANGER), 0);
    lv_obj_add_event_cb(btn_reboot, [](lv_event_t * e) {
        ESP.restart();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l_reboot = lv_label_create(btn_reboot);
    lv_label_set_text(l_reboot, LV_SYMBOL_POWER " Neustart");
    lv_obj_set_style_text_font(l_reboot, &lv_font_montserrat_16, 0);
    lv_obj_center(l_reboot);

    // Right Column: Navigation & Screensaver
    lv_obj_t *l_ui_sec = lv_label_create(t_gen);
    lv_label_set_text(l_ui_sec, "Navigation & Schoner");
    lv_obj_set_style_text_font(l_ui_sec, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_ui_sec, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_ui_sec, LV_ALIGN_TOP_LEFT, 230, 0);

    lv_obj_t *btn_nav_modal = lv_btn_create(t_gen);
    lv_obj_set_size(btn_nav_modal, 210, 46);
    lv_obj_align(btn_nav_modal, LV_ALIGN_TOP_LEFT, 230, 28);
    lv_obj_set_style_radius(btn_nav_modal, ui_radius_button(), 0);
    lv_obj_set_style_bg_color(btn_nav_modal, ui_theme_primary(), 0);
    lv_obj_add_event_cb(btn_nav_modal, [](lv_event_t *) { ui_open_nav_settings(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_nav_m = lv_label_create(btn_nav_modal);
    lv_label_set_text(lbl_nav_m, LV_SYMBOL_LIST " Tabs anpassen");
    lv_obj_set_style_text_font(lbl_nav_m, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_nav_m);

    // Screensaver Switch
    lv_obj_t *sw_scrsave = lv_switch_create(t_gen);
    lv_obj_set_size(sw_scrsave, 48, 26);
    if(state.screensaver_enabled) lv_obj_add_state(sw_scrsave, LV_STATE_CHECKED);
    lv_obj_align(sw_scrsave, LV_ALIGN_TOP_LEFT, 230, 90);
    lv_obj_add_event_cb(sw_scrsave, [](lv_event_t * e) {
        lv_obj_t *sw = lv_event_get_target(e);
        state.screensaver_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        state_save();
    }, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *l_scrsave = lv_label_create(t_gen);
    lv_label_set_text(l_scrsave, "Screensaver");
    lv_obj_set_style_text_font(l_scrsave, &lv_font_montserrat_14, 0);
    lv_obj_align(l_scrsave, LV_ALIGN_TOP_LEFT, 288, 94);

    // Screensaver Brightness
    lv_obj_t *l_scrsave_bri = lv_label_create(t_gen);
    lv_label_set_text_fmt(l_scrsave_bri, "Schoner: %d%%", state.screensaver_brightness);
    lv_obj_set_style_text_font(l_scrsave_bri, &lv_font_montserrat_14, 0);
    lv_obj_align(l_scrsave_bri, LV_ALIGN_TOP_LEFT, 230, 132);

    lv_obj_t *sl_scrsave_bri = lv_slider_create(t_gen);
    lv_slider_set_range(sl_scrsave_bri, 1, 100);
    lv_slider_set_value(sl_scrsave_bri, state.screensaver_brightness, LV_ANIM_OFF);
    lv_obj_set_size(sl_scrsave_bri, 115, 20);
    lv_obj_align(sl_scrsave_bri, LV_ALIGN_TOP_LEFT, 325, 132);
    lv_obj_clear_flag(sl_scrsave_bri, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_user_data(sl_scrsave_bri, l_scrsave_bri);
    lv_obj_add_event_cb(sl_scrsave_bri, [](lv_event_t * e) {
        lv_obj_t *slider = lv_event_get_target(e);
        lv_obj_t *lbl = (lv_obj_t *)lv_obj_get_user_data(slider);
        state.screensaver_brightness = constrain(lv_slider_get_value(slider), 1, 100);
        if (lbl) lv_label_set_text_fmt(lbl, "Schoner: %d%%", state.screensaver_brightness);
        if (ui_screensaver_is_active()) {
            WS_CH32_IO::setPwm(Wire, ui_screensaver_brightness_to_pwm(state.screensaver_brightness));
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sl_scrsave_bri, [](lv_event_t * e) {
        state_save();
    }, LV_EVENT_RELEASED, NULL);
    
    // ==========================================
    // TAB 3: SICHTBARKEIT (ANZEIGE - GROSSE TOGGLE SWITCHES)
    // ==========================================
    lv_obj_add_flag(t_vis, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(t_vis, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(t_vis, 200, 0);
    
    // Helper for creating generous, finger-friendly toggle rows with clickable cards
    auto make_toggle_row = [](lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                              const char *name, bool is_active, auto val_changed_cb, void *user_data,
                              bool is_enabled = true, lv_obj_t **out_card = nullptr, lv_obj_t **out_sw = nullptr, lv_obj_t **out_lbl = nullptr) {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_size(card, w, h);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_bg_color(card, ui_theme_card(), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, ui_theme_border(), 0);
        lv_obj_set_style_pad_hor(card, 8, 0);
        lv_obj_set_style_pad_ver(card, 4, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *sw = lv_switch_create(card);
        lv_obj_set_size(sw, 46, 26);
        lv_obj_align(sw, LV_ALIGN_LEFT_MID, 0, 0);
        if (is_active && is_enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
        else lv_obj_clear_state(sw, LV_STATE_CHECKED);
        if (!is_enabled) lv_obj_add_state(sw, LV_STATE_DISABLED);
        lv_obj_add_event_cb(sw, val_changed_cb, LV_EVENT_VALUE_CHANGED, user_data);

        lv_obj_t *lbl = lv_label_create(card);
        lv_label_set_text(lbl, name);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, is_enabled ? ui_theme_text() : ui_theme_muted(), 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, w - 66);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 54, 0);

        if (!is_enabled) {
            lv_obj_set_style_border_color(card, ui_theme_track(), 0);
            lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
        } else {
            // Entire row is touchable: tapping card toggles the switch
            lv_obj_add_event_cb(card, [](lv_event_t *e) {
                lv_obj_t *s = (lv_obj_t *)lv_event_get_user_data(e);
                if (lv_obj_has_state(s, LV_STATE_DISABLED)) return;
                if (lv_obj_has_state(s, LV_STATE_CHECKED)) lv_obj_clear_state(s, LV_STATE_CHECKED);
                else lv_obj_add_state(s, LV_STATE_CHECKED);
                lv_event_send(s, LV_EVENT_VALUE_CHANGED, NULL);
            }, LV_EVENT_CLICKED, sw);
        }

        if (out_card) *out_card = card;
        if (out_sw) *out_sw = sw;
        if (out_lbl) *out_lbl = lbl;

        return card;
    };

    // 1. Energie Erzeuger (Power Dashboard)
    lv_obj_t *l_pwr_prod = lv_label_create(t_vis);
    lv_label_set_text(l_pwr_prod, "Erzeuger (Power Dashboard)");
    lv_obj_set_style_text_font(l_pwr_prod, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_pwr_prod, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_pwr_prod, LV_ALIGN_TOP_LEFT, 0, 0);

    make_toggle_row(t_vis, 0, 26, 140, 42, "Solar", state.power_show_solar,
        [](lv_event_t *e) {
            lv_obj_t *obj = lv_event_get_target(e);
            state.power_show_solar = lv_obj_has_state(obj, LV_STATE_CHECKED);
            ui_rebuild_power();
        }, NULL, true);

    make_toggle_row(t_vis, 150, 26, 140, 42, "Landstrom", state.power_show_shore,
        [](lv_event_t *e) {
            lv_obj_t *obj = lv_event_get_target(e);
            state.power_show_shore = lv_obj_has_state(obj, LV_STATE_CHECKED);
            ui_rebuild_power();
        }, NULL, state.shore_available, &card_shore_toggle, &sw_shore_toggle, &lbl_shore_toggle);

    make_toggle_row(t_vis, 300, 26, 140, 42, "Lichtm.", state.power_show_alt,
        [](lv_event_t *e) {
            lv_obj_t *obj = lv_event_get_target(e);
            state.power_show_alt = lv_obj_has_state(obj, LV_STATE_CHECKED);
            ui_rebuild_power();
        }, NULL, state.alt_available, &card_alt_toggle, &sw_alt_toggle, &lbl_alt_toggle);

    // Verbraucher-Berechnung Toggle
    make_toggle_row(t_vis, 0, 74, 440, 42, "Verbrauch mit Erzeugern verrechnen", state.power_calc_load,
        [](lv_event_t *e) {
            lv_obj_t *obj = lv_event_get_target(e);
            state.power_calc_load = lv_obj_has_state(obj, LV_STATE_CHECKED);
            state_save();
            ui_rebuild_power();
        }, NULL, true);

    // 2. Schalter & Dimmer
    lv_obj_t *l_vis = lv_label_create(t_vis);
    lv_label_set_text(l_vis, "Schalter & Dimmer Sichtbarkeit");
    lv_obj_set_style_text_font(l_vis, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_vis, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_vis, LV_ALIGN_TOP_LEFT, 0, 128);

    lv_obj_t *l_hdr_relays = lv_label_create(t_vis);
    lv_label_set_text(l_hdr_relays, "Relais (8)");
    lv_obj_set_style_text_font(l_hdr_relays, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l_hdr_relays, ui_theme_muted(), 0);
    lv_obj_align(l_hdr_relays, LV_ALIGN_TOP_LEFT, 4, 156);

    lv_obj_t *l_hdr_dimmers = lv_label_create(t_vis);
    lv_label_set_text(l_hdr_dimmers, "Dimmer (8)");
    lv_obj_set_style_text_font(l_hdr_dimmers, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l_hdr_dimmers, ui_theme_muted(), 0);
    lv_obj_align(l_hdr_dimmers, LV_ALIGN_TOP_LEFT, 229, 156);

    for (int i = 0; i < 8; i++) {
        lv_coord_t row_y = 178 + (i * 48);

        // Relais (Left Column)
        make_toggle_row(t_vis, 0, row_y, 215, 42, state.switch_names[i].c_str(), state.switch_visible[i],
            [](lv_event_t *e) {
                lv_obj_t *obj = lv_event_get_target(e);
                int idx = (int)(intptr_t)lv_event_get_user_data(e);
                state.switch_visible[idx] = lv_obj_has_state(obj, LV_STATE_CHECKED);
            }, (void *)(intptr_t)i);

        // Dimmer (Right Column)
        make_toggle_row(t_vis, 225, row_y, 215, 42, state.dimmer_names[i].c_str(), state.dimmer_visible[i],
            [](lv_event_t *e) {
                lv_obj_t *obj = lv_event_get_target(e);
                int idx = (int)(intptr_t)lv_event_get_user_data(e);
                state.dimmer_visible[idx] = lv_obj_has_state(obj, LV_STATE_CHECKED);
            }, (void *)(intptr_t)i);
    }

    // 3. out-x-in Pro (Relais 9-16) Section at y = 570
    lv_obj_t *l_rext_head = lv_label_create(t_vis);
    lv_label_set_text(l_rext_head, "out-x-in Pro (Relais 9–16)");
    lv_obj_set_style_text_font(l_rext_head, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_rext_head, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_rext_head, LV_ALIGN_TOP_LEFT, 0, 570);

    lv_obj_t *cont_rext = lv_obj_create(t_vis);
    lv_obj_set_size(cont_rext, 440, 210);
    lv_obj_align(cont_rext, LV_ALIGN_TOP_LEFT, 0, 648);
    lv_obj_set_style_bg_opa(cont_rext, 0, 0);
    lv_obj_set_style_border_width(cont_rext, 0, 0);
    lv_obj_set_style_pad_all(cont_rext, 0, 0);
    lv_obj_clear_flag(cont_rext, LV_OBJ_FLAG_SCROLLABLE);
    if (!state.show_relay_ext) lv_obj_add_flag(cont_rext, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *card_master_rext = lv_obj_create(t_vis);
    lv_obj_set_pos(card_master_rext, 0, 596);
    lv_obj_set_size(card_master_rext, 440, 44);
    lv_obj_set_style_radius(card_master_rext, 8, 0);
    lv_obj_set_style_bg_color(card_master_rext, ui_theme_card(), 0);
    lv_obj_set_style_border_width(card_master_rext, 1, 0);
    lv_obj_set_style_border_color(card_master_rext, ui_theme_border(), 0);
    lv_obj_set_style_pad_hor(card_master_rext, 8, 0);
    lv_obj_clear_flag(card_master_rext, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sw_rext = lv_switch_create(card_master_rext);
    lv_obj_set_size(sw_rext, 46, 26);
    lv_obj_align(sw_rext, LV_ALIGN_LEFT_MID, 0, 0);
    if (state.show_relay_ext) lv_obj_add_state(sw_rext, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_rext, [](lv_event_t * e) {
        lv_obj_t *sw = lv_event_get_target(e);
        lv_obj_t *cont = (lv_obj_t *)lv_event_get_user_data(e);
        state.show_relay_ext = lv_obj_has_state(sw, LV_STATE_CHECKED);
        if (state.show_relay_ext) {
            lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
        }
        update_water_relay_dropdowns();
        state_save();
        ui_rebuild_switches();
    }, LV_EVENT_VALUE_CHANGED, cont_rext);

    lv_obj_t *l_sw_rext = lv_label_create(card_master_rext);
    lv_label_set_text(l_sw_rext, "out-x-in Pro aktivieren");
    lv_obj_set_style_text_font(l_sw_rext, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_sw_rext, ui_theme_text(), 0);
    lv_obj_align(l_sw_rext, LV_ALIGN_LEFT_MID, 58, 0);

    lv_obj_add_event_cb(card_master_rext, [](lv_event_t *e) {
        lv_obj_t *s = (lv_obj_t *)lv_event_get_user_data(e);
        if (lv_obj_has_state(s, LV_STATE_CHECKED)) lv_obj_clear_state(s, LV_STATE_CHECKED);
        else lv_obj_add_state(s, LV_STATE_CHECKED);
        lv_event_send(s, LV_EVENT_VALUE_CHANGED, NULL);
    }, LV_EVENT_CLICKED, sw_rext);

    // Relais 9-16 individual toggle rows (0..3 left, 4..7 right)
    for (int i = 0; i < 8; i++) {
        lv_coord_t wx = (i < 4 ? 0 : 225);
        lv_coord_t wy = (i % 4) * 48;
        int sw_idx = 8 + i;
        make_toggle_row(cont_rext, wx, wy, 215, 42, state.switch_names[sw_idx].c_str(), state.switch_visible[sw_idx],
            [](lv_event_t *e) {
                lv_obj_t *obj = lv_event_get_target(e);
                int idx = (int)(intptr_t)lv_event_get_user_data(e);
                state.switch_visible[idx] = lv_obj_has_state(obj, LV_STATE_CHECKED);
            }, (void *)(intptr_t)sw_idx);
    }

    // 4. WiFi Relais Section at y = 870
    lv_obj_t *l_wr_head = lv_label_create(t_vis);
    lv_label_set_text(l_wr_head, "WiFi-Relais");
    lv_obj_set_style_text_font(l_wr_head, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_wr_head, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_wr_head, LV_ALIGN_TOP_LEFT, 0, 870);

    lv_obj_t *cont_wr = lv_obj_create(t_vis);
    lv_obj_set_size(cont_wr, 440, 210);
    lv_obj_align(cont_wr, LV_ALIGN_TOP_LEFT, 0, 948);
    lv_obj_set_style_bg_opa(cont_wr, 0, 0);
    lv_obj_set_style_border_width(cont_wr, 0, 0);
    lv_obj_set_style_pad_all(cont_wr, 0, 0);
    lv_obj_clear_flag(cont_wr, LV_OBJ_FLAG_SCROLLABLE);
    if (!state.show_wrelay) lv_obj_add_flag(cont_wr, LV_OBJ_FLAG_HIDDEN);

    // Master Switch Row for WiFi Relais
    lv_obj_t *card_master_wr = lv_obj_create(t_vis);
    lv_obj_set_pos(card_master_wr, 0, 896);
    lv_obj_set_size(card_master_wr, 440, 44);
    lv_obj_set_style_radius(card_master_wr, 8, 0);
    lv_obj_set_style_bg_color(card_master_wr, ui_theme_card(), 0);
    lv_obj_set_style_border_width(card_master_wr, 1, 0);
    lv_obj_set_style_border_color(card_master_wr, ui_theme_border(), 0);
    lv_obj_set_style_pad_hor(card_master_wr, 8, 0);
    lv_obj_clear_flag(card_master_wr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sw_wrelay = lv_switch_create(card_master_wr);
    lv_obj_set_size(sw_wrelay, 46, 26);
    lv_obj_align(sw_wrelay, LV_ALIGN_LEFT_MID, 0, 0);
    if (state.show_wrelay) lv_obj_add_state(sw_wrelay, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_wrelay, [](lv_event_t * e) {
        lv_obj_t *sw = lv_event_get_target(e);
        lv_obj_t *cont = (lv_obj_t *)lv_event_get_user_data(e);
        state.show_wrelay = lv_obj_has_state(sw, LV_STATE_CHECKED);
        if (state.show_wrelay) {
            lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
        }
        update_water_relay_dropdowns();
    }, LV_EVENT_VALUE_CHANGED, cont_wr);

    lv_obj_t *l_sw_wr = lv_label_create(card_master_wr);
    lv_label_set_text(l_sw_wr, "WiFi-Relais anzeigen");
    lv_obj_set_style_text_font(l_sw_wr, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_sw_wr, ui_theme_text(), 0);
    lv_obj_align(l_sw_wr, LV_ALIGN_LEFT_MID, 58, 0);

    lv_obj_add_event_cb(card_master_wr, [](lv_event_t *e) {
        lv_obj_t *s = (lv_obj_t *)lv_event_get_user_data(e);
        if (lv_obj_has_state(s, LV_STATE_CHECKED)) lv_obj_clear_state(s, LV_STATE_CHECKED);
        else lv_obj_add_state(s, LV_STATE_CHECKED);
        lv_event_send(s, LV_EVENT_VALUE_CHANGED, NULL);
    }, LV_EVENT_CLICKED, sw_wrelay);

    // WiFi Relais individual toggle rows (0..3 left, 4..7 right)
    for (int i = 0; i < 8; i++) {
        lv_coord_t wx = (i < 4 ? 0 : 225);
        lv_coord_t wy = (i % 4) * 48;
        make_toggle_row(cont_wr, wx, wy, 215, 42, state.wrelay_names[i].c_str(), state.wrelay_visible[i],
            [](lv_event_t *e) {
                lv_obj_t *obj = lv_event_get_target(e);
                int idx = (int)(intptr_t)lv_event_get_user_data(e);
                state.wrelay_visible[idx] = lv_obj_has_state(obj, LV_STATE_CHECKED);
            }, (void *)(intptr_t)i);
    }

    // 5. Dimmer Hardware Section at y = 1170
    lv_obj_t *l_dim_head = lv_label_create(t_vis);
    lv_label_set_text(l_dim_head, "Dimmer-Hardware");
    lv_obj_set_style_text_font(l_dim_head, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_dim_head, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_dim_head, LV_ALIGN_TOP_LEFT, 0, 1170);

    lv_obj_t *cont_dimpro = lv_obj_create(t_vis);
    lv_obj_set_size(cont_dimpro, 440, 60);
    lv_obj_align(cont_dimpro, LV_ALIGN_TOP_LEFT, 0, 1248);
    lv_obj_set_style_bg_opa(cont_dimpro, 0, 0);
    lv_obj_set_style_border_width(cont_dimpro, 0, 0);
    lv_obj_set_style_pad_all(cont_dimpro, 0, 0);
    lv_obj_clear_flag(cont_dimpro, LV_OBJ_FLAG_SCROLLABLE);
    if (!state.dimmy_pro) lv_obj_add_flag(cont_dimpro, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *card_master_dim = lv_obj_create(t_vis);
    lv_obj_set_pos(card_master_dim, 0, 1196);
    lv_obj_set_size(card_master_dim, 440, 44);
    lv_obj_set_style_radius(card_master_dim, 8, 0);
    lv_obj_set_style_bg_color(card_master_dim, ui_theme_card(), 0);
    lv_obj_set_style_border_width(card_master_dim, 1, 0);
    lv_obj_set_style_border_color(card_master_dim, ui_theme_border(), 0);
    lv_obj_set_style_pad_hor(card_master_dim, 8, 0);
    lv_obj_clear_flag(card_master_dim, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sw_dimpro = lv_switch_create(card_master_dim);
    lv_obj_set_size(sw_dimpro, 46, 26);
    lv_obj_align(sw_dimpro, LV_ALIGN_LEFT_MID, 0, 0);
    if (state.dimmy_pro) lv_obj_add_state(sw_dimpro, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_dimpro, [](lv_event_t * e) {
        lv_obj_t *sw = lv_event_get_target(e);
        lv_obj_t *cont = (lv_obj_t *)lv_event_get_user_data(e);
        state.dimmy_pro = lv_obj_has_state(sw, LV_STATE_CHECKED);
        if (state.dimmy_pro) {
            lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
        }
        state_save();
        ui_rebuild_dimmers();
    }, LV_EVENT_VALUE_CHANGED, cont_dimpro);

    lv_obj_t *l_sw_dimpro = lv_label_create(card_master_dim);
    lv_label_set_text(l_sw_dimpro, "Dimmy Pro Modus (8–15 + RGBW)");
    lv_obj_set_style_text_font(l_sw_dimpro, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_sw_dimpro, ui_theme_text(), 0);
    lv_obj_align(l_sw_dimpro, LV_ALIGN_LEFT_MID, 58, 0);

    lv_obj_add_event_cb(card_master_dim, [](lv_event_t *e) {
        lv_obj_t *s = (lv_obj_t *)lv_event_get_user_data(e);
        if (lv_obj_has_state(s, LV_STATE_CHECKED)) lv_obj_clear_state(s, LV_STATE_CHECKED);
        else lv_obj_add_state(s, LV_STATE_CHECKED);
        lv_event_send(s, LV_EVENT_VALUE_CHANGED, NULL);
    }, LV_EVENT_CLICKED, sw_dimpro);

    // RGBW 1 and RGBW 2 individual toggle rows inside cont_dimpro
    make_toggle_row(cont_dimpro, 0, 0, 215, 42, state.dimmer_names[8].c_str(), state.dimmer_visible[8],
        [](lv_event_t *e) {
            lv_obj_t *obj = lv_event_get_target(e);
            state.dimmer_visible[8] = lv_obj_has_state(obj, LV_STATE_CHECKED);
        }, (void *)(intptr_t)8);
    make_toggle_row(cont_dimpro, 225, 0, 215, 42, state.dimmer_names[9].c_str(), state.dimmer_visible[9],
        [](lv_event_t *e) {
            lv_obj_t *obj = lv_event_get_target(e);
            state.dimmer_visible[9] = lv_obj_has_state(obj, LV_STATE_CHECKED);
        }, (void *)(intptr_t)9);
    
    // ==========================================
    // TAB 4: WERTE (GROSSE DROPDOWNS & EINGABEN)
    // ==========================================
    lv_obj_add_flag(t_val, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(t_val, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(t_val, 120, 0);
    
    // Section 1: Relais Zuordnung
    lv_obj_t *l_sec_rel = lv_label_create(t_val);
    lv_label_set_text(l_sec_rel, "Relais Zuordnung");
    lv_obj_set_style_text_font(l_sec_rel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_sec_rel, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_sec_rel, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *l_pump = lv_label_create(t_val);
    lv_label_set_text(l_pump, "Pumpe Relais:");
    lv_obj_set_style_text_font(l_pump, &lv_font_montserrat_14, 0);
    lv_obj_align(l_pump, LV_ALIGN_TOP_LEFT, 0, 36);
    
    dd_pump = lv_dropdown_create(t_val);
    lv_obj_set_size(dd_pump, 250, 44);
    lv_obj_set_style_text_font(dd_pump, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_pump, LV_ALIGN_TOP_LEFT, 150, 26);
    lv_obj_add_event_cb(dd_pump, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        int sel = lv_dropdown_get_selected(dd);
        if (sel >= 0 && sel < relay_opt_count) {
            state.pump_relay = relay_opt_map[sel];
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t *l_drain = lv_label_create(t_val);
    lv_label_set_text(l_drain, "Abwasser Relais:");
    lv_obj_set_style_text_font(l_drain, &lv_font_montserrat_14, 0);
    lv_obj_align(l_drain, LV_ALIGN_TOP_LEFT, 0, 90);
    
    dd_drain = lv_dropdown_create(t_val);
    lv_obj_set_size(dd_drain, 250, 44);
    lv_obj_set_style_text_font(dd_drain, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_drain, LV_ALIGN_TOP_LEFT, 150, 80);
    lv_obj_add_event_cb(dd_drain, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        int sel = lv_dropdown_get_selected(dd);
        if (sel >= 0 && sel < relay_opt_count) {
            state.drain_relay = relay_opt_map[sel];
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Section 2: Sensoren & Leistung
    lv_obj_t *l_sec_sens = lv_label_create(t_val);
    lv_label_set_text(l_sec_sens, "Temperatursensoren & Leistung");
    lv_obj_set_style_text_font(l_sec_sens, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_sec_sens, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_sec_sens, LV_ALIGN_TOP_LEFT, 0, 140);

    // Außensensor
    lv_obj_t *l_out_sens = lv_label_create(t_val);
    lv_label_set_text(l_out_sens, "Außensensor:");
    lv_obj_set_style_text_font(l_out_sens, &lv_font_montserrat_14, 0);
    lv_obj_align(l_out_sens, LV_ALIGN_TOP_LEFT, 0, 176);

    dd_out_temp = lv_dropdown_create(t_val);
    lv_obj_set_size(dd_out_temp, 280, 44);
    lv_obj_set_style_text_font(dd_out_temp, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_out_temp, LV_ALIGN_TOP_LEFT, 150, 166);
    lv_obj_add_event_cb(dd_out_temp, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        int sel = lv_dropdown_get_selected(dd);
        if (sel >= 0 && sel < temp_opt_count_primary) {
            state.outdoor_temp_sensor = temp_opt_map_primary[sel];
            if (state.outdoor_temp_sensor >= 0 && state.outdoor_temp_sensor < TEMP_SOURCE_COUNT) {
                state.outdoor_temp = state.temp_sensors[state.outdoor_temp_sensor];
            }
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Innensensor
    lv_obj_t *l_in_sens = lv_label_create(t_val);
    lv_label_set_text(l_in_sens, "Innensensor:");
    lv_obj_set_style_text_font(l_in_sens, &lv_font_montserrat_14, 0);
    lv_obj_align(l_in_sens, LV_ALIGN_TOP_LEFT, 0, 230);

    dd_in_temp = lv_dropdown_create(t_val);
    lv_obj_set_size(dd_in_temp, 280, 44);
    lv_obj_set_style_text_font(dd_in_temp, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_in_temp, LV_ALIGN_TOP_LEFT, 150, 220);
    lv_obj_add_event_cb(dd_in_temp, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        int sel = lv_dropdown_get_selected(dd);
        if (sel >= 0 && sel < temp_opt_count_primary) {
            state.indoor_temp_sensor = temp_opt_map_primary[sel];
            if (state.indoor_temp_sensor >= 0 && state.indoor_temp_sensor < TEMP_SOURCE_COUNT) {
                state.indoor_temp = state.temp_sensors[state.indoor_temp_sensor];
            }
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Kühlschranksensor
    lv_obj_t *l_fridge_sens = lv_label_create(t_val);
    lv_label_set_text(l_fridge_sens, "Kühlschranksensor:");
    lv_obj_set_style_text_font(l_fridge_sens, &lv_font_montserrat_14, 0);
    lv_obj_align(l_fridge_sens, LV_ALIGN_TOP_LEFT, 0, 284);

    dd_fridge_temp = lv_dropdown_create(t_val);
    lv_obj_set_size(dd_fridge_temp, 280, 44);
    lv_obj_set_style_text_font(dd_fridge_temp, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_fridge_temp, LV_ALIGN_TOP_LEFT, 150, 274);
    lv_obj_add_event_cb(dd_fridge_temp, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        int sel = lv_dropdown_get_selected(dd);
        if (sel >= 0 && sel < temp_opt_count_optional) {
            state.fridge_temp_sensor = temp_opt_map_optional[sel];
            if (state.fridge_temp_sensor >= 0 && state.fridge_temp_sensor < TEMP_SOURCE_COUNT) {
                state.fridge_temp = state.temp_sensors[state.fridge_temp_sensor];
            } else {
                state.fridge_temp = -999.0f;
            }
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Zusatzsensor 1
    lv_obj_t *l_cust1_sens = lv_label_create(t_val);
    lv_label_set_text(l_cust1_sens, "Zusatzsensor 1:");
    lv_obj_set_style_text_font(l_cust1_sens, &lv_font_montserrat_14, 0);
    lv_obj_align(l_cust1_sens, LV_ALIGN_TOP_LEFT, 0, 338);

    dd_cust_temp1 = lv_dropdown_create(t_val);
    lv_obj_set_size(dd_cust_temp1, 280, 44);
    lv_obj_set_style_text_font(dd_cust_temp1, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_cust_temp1, LV_ALIGN_TOP_LEFT, 150, 328);
    lv_obj_add_event_cb(dd_cust_temp1, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        int sel = lv_dropdown_get_selected(dd);
        if (sel >= 0 && sel < temp_opt_count_optional) {
            state.temps_custom_sensor[0] = temp_opt_map_optional[sel];
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Zusatzsensor 2
    lv_obj_t *l_cust2_sens = lv_label_create(t_val);
    lv_label_set_text(l_cust2_sens, "Zusatzsensor 2:");
    lv_obj_set_style_text_font(l_cust2_sens, &lv_font_montserrat_14, 0);
    lv_obj_align(l_cust2_sens, LV_ALIGN_TOP_LEFT, 0, 392);

    dd_cust_temp2 = lv_dropdown_create(t_val);
    lv_obj_set_size(dd_cust_temp2, 280, 44);
    lv_obj_set_style_text_font(dd_cust_temp2, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_cust_temp2, LV_ALIGN_TOP_LEFT, 150, 382);
    lv_obj_add_event_cb(dd_cust_temp2, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        int sel = lv_dropdown_get_selected(dd);
        if (sel >= 0 && sel < temp_opt_count_optional) {
            state.temps_custom_sensor[1] = temp_opt_map_optional[sel];
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Populate dropdown options
    update_all_temp_dropdown_options();

    // Solar Max
    lv_obj_t *l_solar = lv_label_create(t_val);
    lv_label_set_text(l_solar, "Solar Max (W):");
    lv_obj_set_style_text_font(l_solar, &lv_font_montserrat_14, 0);
    lv_obj_align(l_solar, LV_ALIGN_TOP_LEFT, 0, 450);

    ta_solar_max = lv_textarea_create(t_val);
    lv_textarea_set_one_line(ta_solar_max, true);
    lv_textarea_set_text(ta_solar_max, String((int)state.solar_max_w).c_str());
    lv_obj_set_size(ta_solar_max, 140, 44);
    lv_obj_set_style_text_font(ta_solar_max, &lv_font_montserrat_14, 0);
    lv_obj_align(ta_solar_max, LV_ALIGN_TOP_LEFT, 150, 440);
    lv_obj_add_event_cb(ta_solar_max, ta_event_cb, LV_EVENT_ALL, NULL);

    // Landstrom Max
    lv_obj_t *l_shore = lv_label_create(t_val);
    lv_label_set_text(l_shore, "Landstrom Max (W):");
    lv_obj_set_style_text_font(l_shore, &lv_font_montserrat_14, 0);
    lv_obj_align(l_shore, LV_ALIGN_TOP_LEFT, 0, 504);

    ta_shore_max = lv_textarea_create(t_val);
    lv_textarea_set_one_line(ta_shore_max, true);
    lv_textarea_set_text(ta_shore_max, String((int)state.shore_max_w).c_str());
    lv_obj_set_size(ta_shore_max, 140, 44);
    lv_obj_set_style_text_font(ta_shore_max, &lv_font_montserrat_14, 0);
    lv_obj_align(ta_shore_max, LV_ALIGN_TOP_LEFT, 150, 494);
    lv_obj_add_event_cb(ta_shore_max, ta_event_cb, LV_EVENT_ALL, NULL);

    // Lichtmaschine Max
    lv_obj_t *l_alt = lv_label_create(t_val);
    lv_label_set_text(l_alt, "Lichtmasch. Max (W):");
    lv_obj_set_style_text_font(l_alt, &lv_font_montserrat_14, 0);
    lv_obj_align(l_alt, LV_ALIGN_TOP_LEFT, 0, 558);

    ta_alt_max = lv_textarea_create(t_val);
    lv_textarea_set_one_line(ta_alt_max, true);
    lv_textarea_set_text(ta_alt_max, String((int)state.alt_max_w).c_str());
    lv_obj_set_size(ta_alt_max, 140, 44);
    lv_obj_set_style_text_font(ta_alt_max, &lv_font_montserrat_14, 0);
    lv_obj_align(ta_alt_max, LV_ALIGN_TOP_LEFT, 150, 548);
    lv_obj_add_event_cb(ta_alt_max, ta_event_cb, LV_EVENT_ALL, NULL);

    // Section 3: Wassertanks Konfiguration
    lv_obj_t *l_tanks = lv_label_create(t_val);
    lv_label_set_text(l_tanks, "Wassertanks Konfiguration");
    lv_obj_set_style_text_font(l_tanks, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_tanks, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_tanks, LV_ALIGN_TOP_LEFT, 0, 608);

    // Anzahl Tanks
    lv_obj_t *l_tank_cnt = lv_label_create(t_val);
    lv_label_set_text(l_tank_cnt, "Anzahl Tanks:");
    lv_obj_set_style_text_font(l_tank_cnt, &lv_font_montserrat_14, 0);
    lv_obj_align(l_tank_cnt, LV_ALIGN_TOP_LEFT, 0, 648);

    dd_tank_count = lv_dropdown_create(t_val);
    lv_dropdown_set_options(dd_tank_count, "1 Tank\n2 Tanks\n3 Tanks\n4 Tanks");
    lv_dropdown_set_selected(dd_tank_count, constrain(state.tank_count, 1, 4) - 1);
    lv_obj_set_size(dd_tank_count, 250, 44);
    lv_obj_set_style_text_font(dd_tank_count, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_tank_count, LV_ALIGN_TOP_LEFT, 150, 638);
    lv_obj_add_event_cb(dd_tank_count, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        state.tank_count = lv_dropdown_get_selected(dd) + 1;
        for (int i = 0; i < 4; i++) {
            state.tank_enabled[i] = (i < state.tank_count);
        }
        update_tank_max_visibility();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Liter anzeigen Toggle
    lv_obj_t *l_tank_liters = lv_label_create(t_val);
    lv_label_set_text(l_tank_liters, "Liter anzeigen:");
    lv_obj_set_style_text_font(l_tank_liters, &lv_font_montserrat_14, 0);
    lv_obj_align(l_tank_liters, LV_ALIGN_TOP_LEFT, 0, 702);

    sw_tank_liters = lv_switch_create(t_val);
    lv_obj_set_size(sw_tank_liters, 52, 28);
    lv_obj_align(sw_tank_liters, LV_ALIGN_TOP_LEFT, 150, 696);
    if (state.tank_show_liters) lv_obj_add_state(sw_tank_liters, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_tank_liters, [](lv_event_t * e) {
        lv_obj_t *sw = lv_event_get_target(e);
        state.tank_show_liters = lv_obj_has_state(sw, LV_STATE_CHECKED);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Tank Maxima & Einheiten
    lv_obj_t *l_tank_caps = lv_label_create(t_val);
    lv_label_set_text(l_tank_caps, "Kapazitäten & Einheit (L / kg):");
    lv_obj_set_style_text_font(l_tank_caps, &lv_font_montserrat_14, 0);
    lv_obj_align(l_tank_caps, LV_ALIGN_TOP_LEFT, 0, 744);

    for (int i = 0; i < 4; i++) {
        int row = i / 2;
        int col = i % 2;
        lv_coord_t col_x = col * 220;
        lv_coord_t row_y = 776 + (row * 54);

        lbl_tank_max[i] = lv_label_create(t_val);
        lv_obj_set_style_text_font(lbl_tank_max[i], &lv_font_montserrat_14, 0);
        lv_obj_align(lbl_tank_max[i], LV_ALIGN_TOP_LEFT, col_x, row_y + 10);

        ta_tank_max[i] = lv_textarea_create(t_val);
        lv_textarea_set_one_line(ta_tank_max[i], true);
        lv_textarea_set_text(ta_tank_max[i], String(state.tank_max[i]).c_str());
        lv_obj_set_size(ta_tank_max[i], 66, 44);
        lv_obj_set_style_text_font(ta_tank_max[i], &lv_font_montserrat_14, 0);
        lv_obj_align(ta_tank_max[i], LV_ALIGN_TOP_LEFT, col_x + 76, row_y);
        lv_obj_add_event_cb(ta_tank_max[i], ta_event_cb, LV_EVENT_ALL, NULL);

        dd_tank_unit[i] = lv_dropdown_create(t_val);
        lv_dropdown_set_options(dd_tank_unit[i], "L\nkg");
        lv_dropdown_set_selected(dd_tank_unit[i], state.tank_unit[i] == 1 ? 1 : 0);
        lv_obj_set_size(dd_tank_unit[i], 66, 44);
        lv_obj_set_style_text_font(dd_tank_unit[i], &lv_font_montserrat_14, 0);
        lv_obj_align(dd_tank_unit[i], LV_ALIGN_TOP_LEFT, col_x + 146, row_y);
        lv_obj_add_event_cb(dd_tank_unit[i], [](lv_event_t * e) {
            lv_obj_t *dd = lv_event_get_target(e);
            int idx = (int)(intptr_t)lv_event_get_user_data(e);
            state.tank_unit[idx] = (uint8_t)lv_dropdown_get_selected(dd);
        }, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);
    }
    update_tank_max_visibility();

    update_water_relay_dropdowns();

    // Section 4: Heizung & Klima
    lv_obj_t *l_sec_heat = lv_label_create(t_val);
    lv_label_set_text(l_sec_heat, "Heizung & Klima");
    lv_obj_set_style_text_font(l_sec_heat, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_sec_heat, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_sec_heat, LV_ALIGN_TOP_LEFT, 0, 848);

    lv_obj_t *l_heat_type = lv_label_create(t_val);
    lv_label_set_text(l_heat_type, "Verbaute Heizung:");
    lv_obj_set_style_text_font(l_heat_type, &lv_font_montserrat_14, 0);
    lv_obj_align(l_heat_type, LV_ALIGN_TOP_LEFT, 0, 892);

    static auto dropdown_to_heater_type = [](int idx) -> uint8_t {
        switch (idx) {
            case 0: return HEATER_TYPE_AUTOTERM;
            case 1: return HEATER_TYPE_TRUMA;
            case 2: return HEATER_TYPE_TRUMA_ELECTRIC;
            case 3: return HEATER_TYPE_WEBASTO;
            case 4: return HEATER_TYPE_NONE;
            default: return HEATER_TYPE_AUTOTERM;
        }
    };

    static auto heater_type_to_dropdown = [](uint8_t type) -> int {
        switch (type) {
            case HEATER_TYPE_AUTOTERM: return 0;
            case HEATER_TYPE_TRUMA: return 1;
            case HEATER_TYPE_TRUMA_ELECTRIC: return 2;
            case HEATER_TYPE_WEBASTO: return 3;
            case HEATER_TYPE_NONE: return 4;
            default: return 0;
        }
    };

    static auto sync_heater_tabs = []() {
        if (is_truma_heater(state.heater_type)) {
            state.tab_enabled[TAB_TRUMA] = true;
            state.tab_enabled[TAB_CLIMATE] = false;
        } else if (state.heater_type == HEATER_TYPE_AUTOTERM || is_webasto_heater(state.heater_type)) {
            state.tab_enabled[TAB_CLIMATE] = true;
            state.tab_enabled[TAB_TRUMA] = false;
        } else {
            state.tab_enabled[TAB_CLIMATE] = false;
            state.tab_enabled[TAB_TRUMA] = false;
        }
    };

    dd_heater_type = lv_dropdown_create(t_val);
    lv_dropdown_set_options(dd_heater_type, "Autoterm (Air 2D/4D)\nTruma (Gas/Diesel)\nTruma + Elektro (Combi E)\nWebasto (W-Bus)\nKeine / Deaktiviert");
    lv_dropdown_set_selected(dd_heater_type, heater_type_to_dropdown(state.heater_type));
    lv_obj_set_size(dd_heater_type, 260, 44);
    lv_obj_set_style_text_font(dd_heater_type, &lv_font_montserrat_14, 0);
    lv_obj_align(dd_heater_type, LV_ALIGN_TOP_LEFT, 150, 882);
    lv_obj_add_event_cb(dd_heater_type, [](lv_event_t * e) {
        lv_obj_t *dd = lv_event_get_target(e);
        int sel = lv_dropdown_get_selected(dd);
        uint8_t ht = HEATER_TYPE_AUTOTERM;
        if (sel == 1) ht = HEATER_TYPE_TRUMA;
        else if (sel == 2) ht = HEATER_TYPE_TRUMA_ELECTRIC;
        else if (sel == 3) ht = HEATER_TYPE_WEBASTO;
        else if (sel == 4) ht = HEATER_TYPE_NONE;
        state.heater_type = ht;
        if (is_truma_heater(state.heater_type)) {
            state.tab_enabled[TAB_TRUMA] = true;
            state.tab_enabled[TAB_CLIMATE] = false;
        } else if (state.heater_type == HEATER_TYPE_AUTOTERM || is_webasto_heater(state.heater_type)) {
            state.tab_enabled[TAB_CLIMATE] = true;
            state.tab_enabled[TAB_TRUMA] = false;
        } else {
            state.tab_enabled[TAB_CLIMATE] = false;
            state.tab_enabled[TAB_TRUMA] = false;
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *l_ht_btns = lv_label_create(t_val);
    lv_label_set_text(l_ht_btns, "+/- Tasten am Arc:");
    lv_obj_set_style_text_font(l_ht_btns, &lv_font_montserrat_14, 0);
    lv_obj_align(l_ht_btns, LV_ALIGN_TOP_LEFT, 0, 946);

    lv_obj_t *sw_ht_btns = lv_switch_create(t_val);
    lv_obj_set_size(sw_ht_btns, 52, 28);
    lv_obj_align(sw_ht_btns, LV_ALIGN_TOP_LEFT, 150, 940);
    if (state.heater_temp_buttons) lv_obj_add_state(sw_ht_btns, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_ht_btns, [](lv_event_t * e) {
        lv_obj_t *sw = lv_event_get_target(e);
        state.heater_temp_buttons = lv_obj_has_state(sw, LV_STATE_CHECKED);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Hint Note
    lv_obj_t *l_out_desc = lv_label_create(t_val);
    lv_label_set_text(l_out_desc, "Hinweis: Der gewählte Außensensor steuert die Frostwarnung & das Außenklima.");
    lv_obj_set_style_text_color(l_out_desc, ui_theme_muted(), 0);
    lv_obj_set_style_text_font(l_out_desc, &lv_font_montserrat_12, 0);
    lv_obj_align(l_out_desc, LV_ALIGN_TOP_LEFT, 0, 988);
    
    // ==========================================
    // TAB 5: ALARME (KOMFORTABLE 44PX EINGABEN)
    // ==========================================
    lv_obj_add_flag(t_alm, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(t_alm, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(t_alm, 80, 0);

    lv_obj_t *l_alm_title = lv_label_create(t_alm);
    lv_label_set_text(l_alm_title, "Smart Header Alarmschwellen");
    lv_obj_set_style_text_font(l_alm_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_alm_title, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_align(l_alm_title, LV_ALIGN_TOP_LEFT, 0, 4);

    // 1. Frost
    lv_obj_t *l_f = lv_label_create(t_alm);
    lv_label_set_text(l_f, "Frostgrenze Außen (<= °C):");
    lv_obj_set_style_text_font(l_f, &lv_font_montserrat_14, 0);
    lv_obj_align(l_f, LV_ALIGN_TOP_LEFT, 0, 46);

    ta_warn_frost = lv_textarea_create(t_alm);
    lv_textarea_set_one_line(ta_warn_frost, true);
    char buf_f[16];
    snprintf(buf_f, sizeof(buf_f), "%.1f", state.warn_frost_temp);
    lv_textarea_set_text(ta_warn_frost, buf_f);
    lv_obj_set_size(ta_warn_frost, 110, 44);
    lv_obj_set_style_text_font(ta_warn_frost, &lv_font_montserrat_14, 0);
    lv_obj_align(ta_warn_frost, LV_ALIGN_TOP_LEFT, 310, 36);
    lv_obj_add_event_cb(ta_warn_frost, ta_event_cb, LV_EVENT_ALL, NULL);

    // 2. Batterie
    lv_obj_t *l_b = lv_label_create(t_alm);
    lv_label_set_text(l_b, "Batterie Rot (<= % SoC):");
    lv_obj_set_style_text_font(l_b, &lv_font_montserrat_14, 0);
    lv_obj_align(l_b, LV_ALIGN_TOP_LEFT, 0, 100);

    ta_warn_bat = lv_textarea_create(t_alm);
    lv_textarea_set_one_line(ta_warn_bat, true);
    lv_textarea_set_accepted_chars(ta_warn_bat, "0123456789");
    lv_textarea_set_max_length(ta_warn_bat, 3);
    lv_textarea_set_text(ta_warn_bat, String(state.warn_bat_soc).c_str());
    lv_obj_set_size(ta_warn_bat, 110, 44);
    lv_obj_set_style_text_font(ta_warn_bat, &lv_font_montserrat_14, 0);
    lv_obj_align(ta_warn_bat, LV_ALIGN_TOP_LEFT, 310, 90);
    lv_obj_add_event_cb(ta_warn_bat, ta_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *l_by = lv_label_create(t_alm);
    lv_label_set_text(l_by, "Batterie Gelb (<= % SoC):");
    lv_obj_set_style_text_font(l_by, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(l_by, 0, 154);
    ta_warn_bat_yellow = lv_textarea_create(t_alm);
    lv_textarea_set_one_line(ta_warn_bat_yellow, true);
    lv_textarea_set_text(ta_warn_bat_yellow, String(state.warn_bat_yellow_soc).c_str());
    lv_textarea_set_accepted_chars(ta_warn_bat_yellow, "0123456789");
    lv_textarea_set_max_length(ta_warn_bat_yellow, 3);
    lv_obj_set_size(ta_warn_bat_yellow, 110, 44);
    lv_obj_set_style_text_font(ta_warn_bat_yellow, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(ta_warn_bat_yellow, 310, 144);
    lv_obj_add_event_cb(ta_warn_bat_yellow, ta_event_cb, LV_EVENT_ALL, nullptr);
    lv_obj_t *battery_note = lv_label_create(t_alm);
    lv_label_set_text(battery_note, "Oberhalb Gelb: Blau. Rot muss kleiner als Gelb sein.");
    lv_obj_set_width(battery_note, 430);
    lv_obj_set_style_text_font(battery_note, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(battery_note, 0, 194);

    // 3. Frischwasser
    lv_obj_t *l_fr = lv_label_create(t_alm);
    lv_label_set_text(l_fr, "Frischwasser Leer (<= %):");
    lv_obj_set_style_text_font(l_fr, &lv_font_montserrat_14, 0);
    lv_obj_align(l_fr, LV_ALIGN_TOP_LEFT, 0, 236);

    ta_warn_fresh = lv_textarea_create(t_alm);
    lv_textarea_set_one_line(ta_warn_fresh, true);
    lv_textarea_set_text(ta_warn_fresh, String(state.warn_fresh_min).c_str());
    lv_obj_set_size(ta_warn_fresh, 110, 44);
    lv_obj_set_style_text_font(ta_warn_fresh, &lv_font_montserrat_14, 0);
    lv_obj_align(ta_warn_fresh, LV_ALIGN_TOP_LEFT, 310, 226);
    lv_obj_add_event_cb(ta_warn_fresh, ta_event_cb, LV_EVENT_ALL, NULL);

    // 4. Abwasser
    lv_obj_t *l_wa = lv_label_create(t_alm);
    lv_label_set_text(l_wa, "Abwasser Voll (>= %):");
    lv_obj_set_style_text_font(l_wa, &lv_font_montserrat_14, 0);
    lv_obj_align(l_wa, LV_ALIGN_TOP_LEFT, 0, 290);

    ta_warn_waste = lv_textarea_create(t_alm);
    lv_textarea_set_one_line(ta_warn_waste, true);
    lv_textarea_set_text(ta_warn_waste, String(state.warn_waste_max).c_str());
    lv_obj_set_size(ta_warn_waste, 110, 44);
    lv_obj_set_style_text_font(ta_warn_waste, &lv_font_montserrat_14, 0);
    lv_obj_align(ta_warn_waste, LV_ALIGN_TOP_LEFT, 310, 280);
    lv_obj_add_event_cb(ta_warn_waste, ta_event_cb, LV_EVENT_ALL, NULL);

    // Virtual Keyboard (Global to scr_settings)
    kb = lv_keyboard_create(scr_settings);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
    
    // ==========================================
    // Back / Cancel & Save Buttons (Footer bar)
    // ==========================================
    lv_obj_t *btn_cancel = lv_btn_create(scr_settings);
    lv_obj_set_size(btn_cancel, 140, 48);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 20, -8);
    lv_obj_set_style_bg_color(btn_cancel, ui_theme_card(), 0);
    lv_obj_set_style_border_width(btn_cancel, 1, 0);
    lv_obj_set_style_border_color(btn_cancel, ui_theme_border(), 0);
    lv_obj_set_style_radius(btn_cancel, 10, 0);
    lv_obj_set_style_shadow_width(btn_cancel, 0, 0);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t * e) {
        (void)e;
        if (kb) {
            lv_keyboard_set_textarea(kb, NULL);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        }
        if (scr_main) lv_scr_load(scr_main);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(l_cancel, LV_SYMBOL_LEFT " Zurück");
    lv_obj_set_style_text_font(l_cancel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_cancel, ui_theme_text(), 0);
    lv_obj_center(l_cancel);

    lv_obj_t *btn_save = lv_btn_create(scr_settings);
    lv_obj_set_size(btn_save, 288, 48);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -20, -8);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(UI_COLOR_SUCCESS), 0);
    lv_obj_set_style_radius(btn_save, 10, 0);
    lv_obj_add_event_cb(btn_save, [](lv_event_t * e) {
        int red = String(lv_textarea_get_text(ta_warn_bat)).toInt();
        int yellow = String(lv_textarea_get_text(ta_warn_bat_yellow)).toInt();
        if (red < 0 || red >= yellow || yellow > 100 ||
            !strlen(lv_textarea_get_text(ta_warn_bat)) || !strlen(lv_textarea_get_text(ta_warn_bat_yellow))) {
            lv_obj_t *error = lv_msgbox_create(nullptr, "Batterie-Schwellen",
                "Bitte 0 <= Rot < Gelb <= 100 eingeben.", nullptr, true);
            if (error) lv_obj_center(error);
            return;
        }
        String victron_error;
        if (!ui_victron_settings_apply(victron_error)) {
            lv_obj_t *error = lv_msgbox_create(nullptr, "Victron GX", victron_error.c_str(), nullptr, true);
            if (error) lv_obj_center(error);
            return;
        }
        state.warn_bat_yellow_soc = yellow;
        state.solar_max_w = String(lv_textarea_get_text(ta_solar_max)).toFloat();
        if (state.solar_max_w <= 0.0f) state.solar_max_w = 200.0f;
        if (ta_shore_max) {
            state.shore_max_w = String(lv_textarea_get_text(ta_shore_max)).toFloat();
            if (state.shore_max_w <= 0.0f) state.shore_max_w = 500.0f;
        }
        if (ta_alt_max) {
            state.alt_max_w = String(lv_textarea_get_text(ta_alt_max)).toFloat();
            if (state.alt_max_w <= 0.0f) state.alt_max_w = 300.0f;
        }
        for (int i=0; i<4; i++) {
            if (ta_tank_max[i]) {
                int val = String(lv_textarea_get_text(ta_tank_max[i])).toInt();
                if (val <= 0) val = 100;
                state.tank_max[i] = val;
            }
            if (dd_tank_unit[i]) {
                state.tank_unit[i] = (uint8_t)lv_dropdown_get_selected(dd_tank_unit[i]);
            }
        }
        if (dd_heater_type) {
            int sel = lv_dropdown_get_selected(dd_heater_type);
            uint8_t ht = HEATER_TYPE_AUTOTERM;
            if (sel == 1) ht = HEATER_TYPE_TRUMA;
            else if (sel == 2) ht = HEATER_TYPE_TRUMA_ELECTRIC;
            else if (sel == 3) ht = HEATER_TYPE_WEBASTO;
            else if (sel == 4) ht = HEATER_TYPE_NONE;
            state.heater_type = ht;
            if (is_truma_heater(state.heater_type)) {
                state.tab_enabled[TAB_TRUMA] = true;
                state.tab_enabled[TAB_CLIMATE] = false;
            } else if (state.heater_type == HEATER_TYPE_AUTOTERM || is_webasto_heater(state.heater_type)) {
                state.tab_enabled[TAB_CLIMATE] = true;
                state.tab_enabled[TAB_TRUMA] = false;
            } else {
                state.tab_enabled[TAB_CLIMATE] = false;
                state.tab_enabled[TAB_TRUMA] = false;
            }
        }
        if (ta_warn_frost) state.warn_frost_temp = String(lv_textarea_get_text(ta_warn_frost)).toFloat();
        if (ta_warn_bat)   state.warn_bat_soc    = String(lv_textarea_get_text(ta_warn_bat)).toInt();
        if (ta_warn_fresh) state.warn_fresh_min  = String(lv_textarea_get_text(ta_warn_fresh)).toInt();
        if (ta_warn_waste) state.warn_waste_max  = String(lv_textarea_get_text(ta_warn_waste)).toInt();
        if (dd_out_temp) {
            int sel = lv_dropdown_get_selected(dd_out_temp);
            if (sel >= 0 && sel < temp_opt_count_primary) {
                state.outdoor_temp_sensor = temp_opt_map_primary[sel];
            }
        }
        if (dd_in_temp) {
            int sel = lv_dropdown_get_selected(dd_in_temp);
            if (sel >= 0 && sel < temp_opt_count_primary) {
                state.indoor_temp_sensor = temp_opt_map_primary[sel];
            }
        }
        if (dd_cust_temp1) {
            int sel = lv_dropdown_get_selected(dd_cust_temp1);
            if (sel >= 0 && sel < temp_opt_count_optional) {
                state.temps_custom_sensor[0] = temp_opt_map_optional[sel];
            }
        }
        if (dd_cust_temp2) {
            int sel = lv_dropdown_get_selected(dd_cust_temp2);
            if (sel >= 0 && sel < temp_opt_count_optional) {
                state.temps_custom_sensor[1] = temp_opt_map_optional[sel];
            }
        }
        if (dd_fridge_temp) {
            int sel = lv_dropdown_get_selected(dd_fridge_temp);
            if (sel >= 0 && sel < temp_opt_count_optional) {
                state.fridge_temp_sensor = temp_opt_map_optional[sel];
            }
        }
        if (state.fridge_temp_sensor >= 0 && state.fridge_temp_sensor < TEMP_SOURCE_COUNT) {
            state.fridge_temp = state.temp_sensors[state.fridge_temp_sensor];
        } else {
            state.fridge_temp = -999.0f;
        }
        if (state.outdoor_temp_sensor >= 0 && state.outdoor_temp_sensor < TEMP_SOURCE_COUNT) {
            state.outdoor_temp = state.temp_sensors[state.outdoor_temp_sensor];
        }
        if (state.indoor_temp_sensor >= 0 && state.indoor_temp_sensor < TEMP_SOURCE_COUNT) {
            state.indoor_temp = state.temp_sensors[state.indoor_temp_sensor];
        }
        save_settings_cb(e);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l_save = lv_label_create(btn_save);
    lv_label_set_text(l_save, LV_SYMBOL_OK " Speichern & Beenden");
    lv_obj_set_style_text_font(l_save, &lv_font_montserrat_16, 0);
    lv_obj_center(l_save);
}
