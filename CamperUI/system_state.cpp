#include "radio_tab_migration.h"
#include "system_state.h"
#include "victron_client.h"
#include "settings_migration.h"
#include "display_sync.h"
#include <time.h>
#include <esp_sntp.h>

// Optional local credentials; builds without this file remain supported.
#if __has_include("config/wifi_secrets.h")
#include "config/wifi_secrets.h"
#endif
#ifndef CAMPERUI_DEFAULT_WIFI_SSID
#define CAMPERUI_DEFAULT_WIFI_SSID ""
#endif
#ifndef CAMPERUI_DEFAULT_WIFI_PASSWORD
#define CAMPERUI_DEFAULT_WIFI_PASSWORD ""
#endif

SystemState state;
Preferences prefs;
SemaphoreHandle_t state_mutex = NULL;

// Pre-configuration block in ROM: patchable in browser before flashing
__attribute__((used, section(".rodata.camperui_cfg")))
const volatile PreFlashingConfig PRE_FLASH_CONFIG = {
    "CAMPERUI_CFG_V1",
    0, // is_configured
    "__CAMPERUI_SSID_PLACEHOLDER___________________________________",
    "__CAMPERUI_PASS_PLACEHOLDER___________________________________",
    "__CAMPERUI_VANPI_PLACEHOLDER__________________________________",
    -1, // display_brightness
    -1, // display_rotation
    -1, // dark_mode
    {0}
};

void state_init() {
    if (!state_mutex) {
        state_mutex = xSemaphoreCreateRecursiveMutex();
    }
    prefs.begin("camperui", false);

    // Load persisted settings
    state.wifi_ssid = prefs.getString("wifi_ssid", "");
    state.wifi_pass = prefs.getString("wifi_pass", "");

    // Check if binary was pre-configured via Web Flasher before flashing
    if (state.wifi_ssid.length() == 0 && PRE_FLASH_CONFIG.is_configured == 1 &&
        strncmp((const char *)PRE_FLASH_CONFIG.magic, "CAMPERUI_CFG_V1", 15) == 0) {
        if (PRE_FLASH_CONFIG.wifi_ssid[0] != '\0' && strncmp((const char *)PRE_FLASH_CONFIG.wifi_ssid, "__CAMPERUI_", 11) != 0) {
            state.wifi_ssid = String((const char *)PRE_FLASH_CONFIG.wifi_ssid);
            prefs.putString("wifi_ssid", state.wifi_ssid);
        }
        if (PRE_FLASH_CONFIG.wifi_pass[0] != '\0' && strncmp((const char *)PRE_FLASH_CONFIG.wifi_pass, "__CAMPERUI_", 11) != 0) {
            state.wifi_pass = String((const char *)PRE_FLASH_CONFIG.wifi_pass);
            prefs.putString("wifi_pass", state.wifi_pass);
        }
        if (PRE_FLASH_CONFIG.vanpi_ip[0] != '\0' && strncmp((const char *)PRE_FLASH_CONFIG.vanpi_ip, "__CAMPERUI_", 11) != 0) {
            state.vanpi_ip = String((const char *)PRE_FLASH_CONFIG.vanpi_ip);
            prefs.putString("vanpi_ip", state.vanpi_ip);
        }
        if (PRE_FLASH_CONFIG.display_brightness >= 10 && PRE_FLASH_CONFIG.display_brightness <= 100) {
            prefs.putInt("disp_bright", PRE_FLASH_CONFIG.display_brightness);
        }
        if (PRE_FLASH_CONFIG.display_rotation >= 0 && PRE_FLASH_CONFIG.display_rotation <= 3) {
            prefs.putInt("disp_rot", PRE_FLASH_CONFIG.display_rotation);
        }
        if (PRE_FLASH_CONFIG.dark_mode >= 0) {
            prefs.putBool("dark_mode", PRE_FLASH_CONFIG.dark_mode == 1);
        }
    }

    // Seed only an unconfigured device. Saved settings always take priority.
    if (state.wifi_ssid.length() == 0 && CAMPERUI_DEFAULT_WIFI_SSID[0] != '\0') {
        state.wifi_ssid = CAMPERUI_DEFAULT_WIFI_SSID;
        state.wifi_pass = CAMPERUI_DEFAULT_WIFI_PASSWORD;
        prefs.putString("wifi_ssid", state.wifi_ssid);
        prefs.putString("wifi_pass", state.wifi_pass);
    }
    state.vanpi_ip = prefs.getString("vanpi_ip", "100.80.161.23");

    
    state.display_brightness = prefs.getInt("disp_bright", 80);
    if (state.display_brightness > 100) state.display_brightness = 100;
    
    state.display_timeout = prefs.getInt("disp_timeout", 30);
    state.display_rotation = prefs.getInt("disp_rot", 2);
    state.dark_mode = prefs.getBool("dark_mode", true);
    state.battery_icon_mode = prefs.getBool("bat_icn_md", true);
    state.buzzer_enabled = prefs.getBool("buzzer_en", true);
    state.screensaver_enabled = prefs.getBool("scrsave_en", false);
    state.screensaver_brightness = constrain(prefs.getInt("scrsave_brt", 5), 1, 100);
    state.debug_mode = prefs.getBool("dbg_sim", true);

    ThemeConfig saved_theme;
    size_t theme_sz = prefs.getBytes("theme_cfg", &saved_theme, sizeof(ThemeConfig));
    if (theme_sz == sizeof(ThemeConfig) && saved_theme.version == 3) {
        state.theme = saved_theme;
    } else if (saved_theme.version == 2) {
        state.theme = saved_theme;
        state.theme.version = 3;
        state.theme.color_btn_text = saved_theme.color_text ? saved_theme.color_text : 0xf1f5f9;
        state.theme.color_btn_text_active = 0xffffff;
    } else {
        theme_config_init_default(state.theme);
    }
    
    state.bat_capacity_ah = prefs.getFloat("bat_cap_ah", 100.0f);
    state.solar_max_w = prefs.getFloat("solar_max_w", 200.0f);
    state.shore_max_w = prefs.getFloat("shore_max_w", 500.0f);
    state.alt_max_w = prefs.getFloat("alt_max_w", 300.0f);

    state.warn_frost_temp = prefs.getFloat("w_frost", 3.0f);
    state.warn_bat_soc = constrain(prefs.getInt("w_bat", 20), 0, 99);
    state.warn_bat_yellow_soc = constrain(prefs.getInt("w_bat_y", 40), state.warn_bat_soc + 1, 100);
    state.warn_fresh_min = prefs.getInt("w_fresh", 15);
    state.warn_waste_max = prefs.getInt("w_waste", 85);

    state.outdoor_temp_sensor = constrain(prefs.getInt("out_temp_idx", 1), 0, TEMP_SOURCE_COUNT - 1);
    state.indoor_temp_sensor = constrain(prefs.getInt("in_temp_idx", 0), 0, TEMP_SOURCE_COUNT - 1);
    state.fridge_temp_sensor = constrain(prefs.getInt("fridge_t_idx", -1), -1, TEMP_SOURCE_COUNT - 1);
    state.temps_custom_sensor[0] = constrain(prefs.getInt("t_cust0", -1), -1, TEMP_SOURCE_COUNT - 1);
    state.temps_custom_sensor[1] = constrain(prefs.getInt("t_cust1", -1), -1, TEMP_SOURCE_COUNT - 1);
    state.temps_history_window = prefs.getInt("t_win", 1);
    if (state.temps_history_window != 1 && state.temps_history_window != 6 && state.temps_history_window != 12) {
        state.temps_history_window = 1;
    }
    const int home_defaults[3] = {0, 1, 3};
    for (int i = 0; i < 3; ++i) {
        char key[16]; snprintf(key, sizeof(key), "home_temp%d", i);
        bool was_visible = prefs.getInt(key, home_defaults[i]) >= 0;
        snprintf(key, sizeof(key), "home_tshow%d", i);
        state.home_show_temp[i] = prefs.getBool(key, was_visible);
    }
    for (int i = 0; i < 2; ++i) {
        char key[16]; snprintf(key, sizeof(key), "home_fav%d", i);
        state.home_favorite[i] = constrain(prefs.getInt(key, i == 0 ? 1 : 3), 0, 18);
    }
    state.home_show_battery = prefs.getBool("home_bat", true);
    state.home_show_starter = prefs.getBool("home_starter", true);
    state.home_show_solar = prefs.getBool("home_solar", true);
    state.home_show_water = prefs.getBool("home_water", true);
    state.home_show_victron = prefs.getBool("home_gx", false);
    if (state.home_show_victron) state.home_show_water = false;
    state.victron_enabled = prefs.getBool("gx_en", false);
    state.victron_keepalive = prefs.getBool("gx_ka", false);
    state.victron_starter = prefs.getBool("gx_starter", false);
    state.victron_battery_instance = constrain(prefs.getInt("gx_bat_inst", -1), -1, 65535);
    state.victron_host = prefs.getString("gx_host", "");
    state.victron_portal = prefs.getString("gx_portal", "");
    state.victron_port = constrain(prefs.getUInt("gx_port", 1883), 1, 65535);
    state.victron_instance = constrain(prefs.getInt("gx_inst", -1), -1, 65535);
    state.victron_user = prefs.getString("gx_user", "");
    state.victron_password = prefs.getString("gx_pass", "");
    state.victron_relay_names[0] = prefs.getString("gx_r0", "GX Relais 1");
    state.victron_relay_names[1] = prefs.getString("gx_r1", "GX Relais 2");
    state.home_water_source = constrain(prefs.getInt("home_tank", 0), 0, 3);

    // Power Dashboard Producers & Calculation
    state.power_show_solar = prefs.getBool("pwr_solar", true);
    state.power_show_shore = prefs.getBool("pwr_shore", true);
    state.power_show_alt   = prefs.getBool("pwr_alt", true);
    state.power_calc_load  = prefs.getBool("pwr_calcload", false);

    // Tab Order & Visibility: preserve legacy RV3D and upstream page identities.
    const uint8_t rv_cfg_version=prefs.getUChar("rv_cfg_v",0);
    const bool legacy_rv_tabs=prefs.isKey("gx_en") && rv_cfg_version<2;
    size_t r_order = prefs.getBytes("tab_order", state.tab_order, sizeof(state.tab_order));
    const size_t saved_order_bytes=r_order;
    r_order = radio_migrate_tab_order(state.tab_order, r_order, legacy_rv_tabs);
    if (r_order != sizeof(state.tab_order)) {
        for (uint8_t i = 0; i < TAB_COUNT; ++i) state.tab_order[i] = i;
    } else {
        bool seen[TAB_COUNT] = {false};
        bool valid = true;
        for (uint8_t i = 0; i < TAB_COUNT; ++i) {
            if (state.tab_order[i] >= TAB_COUNT || seen[state.tab_order[i]]) {
                valid = false;
                break;
            }
            seen[state.tab_order[i]] = true;
        }
        if (!valid) {
            for (uint8_t i = 0; i < TAB_COUNT; ++i) state.tab_order[i] = i;
        }
    }

    size_t r_en = prefs.getBytes("tab_en", state.tab_enabled, sizeof(state.tab_enabled));
    r_en = radio_migrate_tab_visibility(state.tab_enabled, r_en, legacy_rv_tabs);
    if (r_en != sizeof(state.tab_enabled)) {
        for (uint8_t i = 0; i < TAB_COUNT; ++i) state.tab_enabled[i] = (i != TAB_VICTRON && i != TAB_FRIDGE);
    }

    state.tank_count = prefs.getInt("tank_cnt", 2);
    if (state.tank_count < 1 || state.tank_count > 4) state.tank_count = 2;
    state.tank_show_liters = prefs.getBool("tank_liters", true);

    for (int i = 0; i < 4; i++) {
        char key_max[16], key_en[16], key_nm[16], key_un[16];
        sprintf(key_max, "tank%d_max", i);
        sprintf(key_en, "tank%d_en", i);
        sprintf(key_nm, "tank%d_nm", i);
        sprintf(key_un, "tank%d_un", i);
        state.tank_max[i] = prefs.getInt(key_max, 100);
        state.tank_unit[i] = constrain(prefs.getInt(key_un, 0), 0, 1);
        state.tank_enabled[i] = (i < state.tank_count);
        
        String def_name = "";
        if (i == 0) def_name = "Frischwasser";
        else if (i == 1) def_name = "Grauwasser";
        else if (i == 2) def_name = "Tank 3";
        else def_name = "Tank 4";
        state.tank_names[i] = prefs.getString(key_nm, def_name);
    }

    for (int i = 0; i < 16; i++) {
        char key_sw[16], key_sw_vis[16], key_sw_lock[16];
        sprintf(key_sw, "sw%d_nm", i);
        sprintf(key_sw_vis, "sw%d_vis", i);
        sprintf(key_sw_lock, "sw%d_lock", i);
        state.switch_names[i] = prefs.getString(key_sw, "CH " + String(i + 1));
        state.switch_visible[i] = prefs.getBool(key_sw_vis, true);
        state.switch_locked[i] = prefs.getBool(key_sw_lock, false);
    }
    for (int i = 0; i < 10; i++) {
        char key_dim[16], key_dim_vis[16], key_wr_lock[16];
        sprintf(key_dim, "dim%d_nm", i);
        sprintf(key_dim_vis, "dim%d_vis", i);
        String def_dim = (i < 8) ? ("PWM " + String(i + 1)) : ("RGBW " + String(i - 7));
        state.dimmer_names[i] = prefs.getString(key_dim, def_dim);
        state.dimmer_visible[i] = prefs.getBool(key_dim_vis, true);
        if (i < 8) {
            sprintf(key_wr_lock, "wr%d_lock", i);
            state.wrelay_locked[i] = prefs.getBool(key_wr_lock, false);
        }
    }
    state.pump_relay = prefs.getInt("pump_relay", -1);
    state.drain_relay = prefs.getInt("drain_relay", -1);
    if (rv_cfg_version < 3) {
        if (state.pump_relay >= 8 && state.pump_relay <= 15) state.pump_relay += 8;
        if (state.drain_relay >= 8 && state.drain_relay <= 15) state.drain_relay += 8;
    }

    // Initialize defaults for volatile data
    state.wifi_connected = false;
    
    state.wifi_rssi = 0;
    state.vanpi_connected = false;

    state.bat_voltage = 12.0f;
    state.bat_current = 0.0f;
    state.bat_soc = 0;
    state.solar_power = 0.0f;
    state.solar_current = 0.0f;
    state.solar_voltage = 0.0f;
    state.starter_voltage = 0.0f;
    state.shore_power = 0.0f;
    state.shore_voltage = 0.0f;
    state.shore_current = 0.0f;
    state.shore_connected = false;
    state.shore_available = false;
    state.alt_power = 0.0f;
    state.alt_current = 0.0f;
    state.alt_active = false;
    state.alt_available = false;
    state.battery_fields = state.solar_fields = state.temp_fields = state.tank_fields = 0;
    state.relay_fields = state.dimmer_fields = 0;
    state.data_is_demo = state.debug_mode;

    for (int i = 0; i < 4; i++) {
        state.tank_level[i] = 0;
        state.temp_sensors[i] = 0.0f;
        state.temp_sensor_names[i] = "Sensor " + String(i + 1);
    }

    for (int i = 0; i < TEMP_SOURCE_COUNT; ++i) {
        state.temp_sensors[i] = 0;
        state.temp_humidity[i] = 0.0f;
        state.temp_is_humidity[i] = false;
        if (i >= 4) state.temp_sensor_names[i] = "Ruuvi " + String(i - 4);
    }
    state.indoor_temp = 0.0f;
    state.outdoor_temp = 0.0f;
    state.fridge_temp = -999.0f;
    state.indoor_humidity = 0.0f;
    state.target_temp = 20.0f;
    state.heating_on = false;
    state.heater_vent_mode = false;

    // Dometic RC10 Fridge defaults
    state.fridge_power = false;
    state.fridge_mode = 0; // performance
    state.fridge_step = 3;
    strncpy(state.fridge_power_source, "DC12", sizeof(state.fridge_power_source));
    state.fridge_compressor = false;
    state.fridge_fan = false;
    strncpy(state.fridge_error, "none", sizeof(state.fridge_error));
    state.fridge_last_update = 0;
    state.fridge_hold_until = 0;
    state.heater_power_mode = false;
    state.heater_power_level = 5;
    state.heater_diagnostics = HeaterDiagnostics();
    state.heater_status = "";
    state.heater_error = "no";
    state.fan_on = false;
    state.webasto_temp = 0.0f;
    state.webasto_voltage = 0.0f;
    state.heater_type = constrain(rv_migrate_heater_type(prefs.getInt("heater_type", HEATER_TYPE_AUTOTERM),
        rv_cfg_version<2 && (legacy_rv_tabs || saved_order_bytes<=10)), 0, 4);
    state.heater_temp_buttons = prefs.getBool("ht_btns", false);

    // Truma Climate & Boiler defaults
    state.truma_online = false;
    state.truma_model = "Truma Combi";
    state.truma_heating_on = false;
    state.truma_target_temp = 22.0f;
    state.truma_air_mode = "normal";
    state.truma_water_mode = "off";
    state.truma_energy_mode = "fuel";
    state.truma_power_limit = 900;
    state.truma_230v = false;
    state.truma_error = "none";
    state.truma_hold_until = 0;

    // Sync tab visibility with heater_type
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

    state.pitch_angle = 0.0f;
    state.roll_angle = 0.0f;

    for (int i = 0; i < 16; i++) {
        state.switch_state[i] = false;
        state.relay_hold_until[i] = 0;
    }
    for (int i = 0; i < 10; i++) {
        state.dimmer_val[i] = 0;
        state.dimmer_hold_until[i] = 0;
    }
    state.heater_hold_until = 0;

    state.vanpi_current_date = "";
    state.time_to_go = "";

    state.show_wrelay = prefs.getBool("show_wrelay", false);
    state.show_relay_ext = prefs.getBool("show_r_ext", false);
    state.dimmy_pro = prefs.getBool("dimmy_pro", false);
    for (int i = 0; i < 8; i++) {
        char key_w_nm[16], key_w_vis[16];
        sprintf(key_w_nm, "wr%d_nm", i);
        sprintf(key_w_vis, "wr%d_vis", i);
        state.wrelay_names[i] = prefs.getString(key_w_nm, "W-Relais " + String(i + 1));
        state.wrelay_visible[i] = prefs.getBool(key_w_vis, true);
        state.wrelay_state[i] = false;
        state.wrelay_hold_until[i] = 0;
    }
    state.wrelay_fields = 0;

    state.web_ota_active = false;
    state.web_ota_progress = 0;
    state.web_ota_msg = "Bereit";
}

static void put_string_if_changed(const char *key, const String &val) {
    if (prefs.getString(key, "\x01") != val) {
        prefs.putString(key, val);
    }
}

static void put_bytes_if_changed(const char *key, const void *buf, size_t len) {
    if (len <= 128) {
        uint8_t cur[128];
        if (prefs.getBytes(key, cur, len) == len && memcmp(cur, buf, len) == 0) {
            return;
        }
    }
    prefs.putBytes(key, buf, len);
}

void state_save() {
    victron_settings_changed();
    StateLockGuard lock;
    for (int i = 0; i < 3; ++i) {
        char key[16]; snprintf(key, sizeof(key), "home_tshow%d", i);
        prefs.putBool(key, state.home_show_temp[i]);
    }
    for (int i = 0; i < 2; ++i) {
        char key[16]; snprintf(key, sizeof(key), "home_fav%d", i);
        prefs.putInt(key, state.home_favorite[i]);
    }
    prefs.putBool("home_bat", state.home_show_battery);
    prefs.putBool("home_starter", state.home_show_starter);
    prefs.putBool("home_solar", state.home_show_solar);
    prefs.putBool("home_water", state.home_show_water);
    prefs.putBool("home_gx", state.home_show_victron);
    prefs.putBool("gx_en", state.victron_enabled);
    prefs.putBool("gx_ka", state.victron_keepalive);
    prefs.putBool("gx_starter", state.victron_starter);
    prefs.putInt("gx_bat_inst", state.victron_battery_instance);
    put_string_if_changed("gx_host", state.victron_host);
    put_string_if_changed("gx_portal", state.victron_portal);
    prefs.putUInt("gx_port", state.victron_port);
    prefs.putInt("gx_inst", state.victron_instance);
    put_string_if_changed("gx_user", state.victron_user);
    put_string_if_changed("gx_pass", state.victron_password);
    put_string_if_changed("gx_r0", state.victron_relay_names[0]);
    put_string_if_changed("gx_r1", state.victron_relay_names[1]);
    prefs.putInt("home_tank", state.home_water_source);
    prefs.putBool("pwr_solar", state.power_show_solar);
    prefs.putBool("pwr_shore", state.power_show_shore);
    prefs.putBool("pwr_alt", state.power_show_alt);
    prefs.putBool("pwr_calcload", state.power_calc_load);
    put_string_if_changed("wifi_ssid", state.wifi_ssid);
    put_string_if_changed("wifi_pass", state.wifi_pass);
    put_string_if_changed("vanpi_ip", state.vanpi_ip);
    prefs.putInt("disp_bright", state.display_brightness);
    prefs.putInt("disp_timeout", state.display_timeout);
    prefs.putInt("disp_rot", state.display_rotation);
    prefs.putBool("dark_mode", state.dark_mode);
    prefs.putBool("bat_icn_md", state.battery_icon_mode);
    prefs.putBool("buzzer_en", state.buzzer_enabled);
    prefs.putBool("dbg_sim", state.debug_mode);
    
    prefs.putFloat("bat_cap_ah", state.bat_capacity_ah);
    prefs.putFloat("solar_max_w", state.solar_max_w);
    prefs.putFloat("shore_max_w", state.shore_max_w);
    prefs.putFloat("alt_max_w", state.alt_max_w);

    prefs.putFloat("w_frost", state.warn_frost_temp);
    prefs.putInt("w_bat", state.warn_bat_soc);
    prefs.putInt("w_bat_y", state.warn_bat_yellow_soc);
    prefs.putInt("w_fresh", state.warn_fresh_min);
    prefs.putInt("w_waste", state.warn_waste_max);

    prefs.putInt("out_temp_idx", state.outdoor_temp_sensor);
    prefs.putInt("in_temp_idx", state.indoor_temp_sensor);
    prefs.putInt("fridge_t_idx", state.fridge_temp_sensor);
    prefs.putInt("t_cust0", state.temps_custom_sensor[0]);
    prefs.putInt("t_cust1", state.temps_custom_sensor[1]);
    prefs.putInt("t_win", state.temps_history_window);
    put_bytes_if_changed("tab_order", state.tab_order, sizeof(state.tab_order));
    put_bytes_if_changed("tab_en", state.tab_enabled, sizeof(state.tab_enabled));

    prefs.putInt("tank_cnt", state.tank_count);
    prefs.putBool("tank_liters", state.tank_show_liters);
    for (int i = 0; i < 4; i++) {
        char key_max[16], key_en[16], key_nm[16], key_un[16];
        sprintf(key_max, "tank%d_max", i);
        sprintf(key_en, "tank%d_en", i);
        sprintf(key_nm, "tank%d_nm", i);
        sprintf(key_un, "tank%d_un", i);
        prefs.putInt(key_max, state.tank_max[i]);
        prefs.putBool(key_en, state.tank_enabled[i]);
        put_string_if_changed(key_nm, state.tank_names[i]);
        prefs.putInt(key_un, state.tank_unit[i]);
    }
    
    for (int i = 0; i < 16; i++) {
        char key_sw[16], key_sw_vis[16];
        sprintf(key_sw, "sw%d_nm", i);
        sprintf(key_sw_vis, "sw%d_vis", i);
        put_string_if_changed(key_sw, state.switch_names[i]);
        prefs.putBool(key_sw_vis, state.switch_visible[i]);
    }
    for (int i = 0; i < 10; i++) {
        char key_dim[16], key_dim_vis[16];
        sprintf(key_dim, "dim%d_nm", i);
        sprintf(key_dim_vis, "dim%d_vis", i);
        put_string_if_changed(key_dim, state.dimmer_names[i]);
        prefs.putBool(key_dim_vis, state.dimmer_visible[i]);
    }
    prefs.putInt("pump_relay", state.pump_relay);
    prefs.putInt("drain_relay", state.drain_relay);
    prefs.putInt("heater_type", state.heater_type);
    prefs.putBool("ht_btns", state.heater_temp_buttons);
    prefs.putUChar("rv_cfg_v", 3);

    prefs.putBool("scrsave_en", state.screensaver_enabled);
    prefs.putInt("scrsave_brt", state.screensaver_brightness);

    prefs.putBool("show_wrelay", state.show_wrelay);
    prefs.putBool("show_r_ext", state.show_relay_ext);
    prefs.putBool("dimmy_pro", state.dimmy_pro);
    for (int i = 0; i < 8; i++) {
        char key_w_nm[16], key_w_vis[16];
        sprintf(key_w_nm, "wr%d_nm", i);
        sprintf(key_w_vis, "wr%d_vis", i);
        put_string_if_changed(key_w_nm, state.wrelay_names[i]);
        prefs.putBool(key_w_vis, state.wrelay_visible[i]);
    }

    put_bytes_if_changed("theme_cfg", &state.theme, sizeof(ThemeConfig));

    display_request_resync();
}

void state_save_theme() {
    StateLockGuard lock;
    put_bytes_if_changed("theme_cfg", &state.theme, sizeof(ThemeConfig));
    display_request_resync();
}

void state_set_switch_locked(int idx, bool locked) {
    if (idx < 0 || idx >= 16) return;
    StateLockGuard lock;
    state.switch_locked[idx] = locked;
    char key[16];
    snprintf(key, sizeof(key), "sw%d_lock", idx);
    prefs.putBool(key, locked);
    display_request_resync();
}

void state_set_wrelay_locked(int idx, bool locked) {
    if (idx < 0 || idx >= 8) return;
    StateLockGuard lock;
    state.wrelay_locked[idx] = locked;
    char key[16];
    snprintf(key, sizeof(key), "wr%d_lock", idx);
    prefs.putBool(key, locked);
    display_request_resync();
}

