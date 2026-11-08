#include "web_settings.h"
#include "radio_settings.h"
#include "powerunit_settings.h"
#include "victron_settings.h"
#include "victron_client.h"
#include "web_settings_html.h"
#include "system_state.h"
#include "ui_main.h"
#include "wifi_diagnostics.h"
#include "WS_CH32_IO.h"
#include "ui_screensaver.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>

extern TwoWire Wire;

void web_settings_register_routes(WebServer &server) {
    radio_register_routes(server);
    powerunit_register_routes(server);
    // 1. Settings Web UI
    server.on("/settings", HTTP_GET, [&server]() {
        server.send_P(200, "text/html; charset=utf-8", SETTINGS_HTML);
    });

    // 2. GET /api/settings
    server.on("/api/settings", HTTP_GET, [&server]() {
        JsonDocument doc;
        {
            StateLockGuard lock;
            doc["version"] = CAMPERUI_VERSION;
            doc["wifi_ssid"] = state.wifi_ssid;
            doc["wifi_pass"] = state.wifi_pass;
            doc["vanpi_ip"] = state.vanpi_ip;

            doc["dark_mode"] = state.dark_mode;
            doc["buzzer_enabled"] = state.buzzer_enabled;
            doc["debug_mode"] = state.debug_mode;
            doc["display_brightness"] = state.display_brightness;
            doc["display_timeout"] = state.display_timeout;
            doc["screensaver_enabled"] = state.screensaver_enabled;
            doc["screensaver_brightness"] = state.screensaver_brightness;

            doc["tank_count"] = state.tank_count;
            doc["tank_show_liters"] = state.tank_show_liters;
            JsonArray t_max = doc["tank_max"].to<JsonArray>();
            for (int i = 0; i < 4; i++) t_max.add(state.tank_max[i]);
            JsonArray t_un = doc["tank_unit"].to<JsonArray>();
            for (int i = 0; i < 4; i++) t_un.add(state.tank_unit[i]);

            doc["pump_relay"] = state.pump_relay;
            doc["drain_relay"] = state.drain_relay;
            doc["heater_type"] = state.heater_type;
            doc["heater_temp_buttons"] = state.heater_temp_buttons;

            doc["show_wrelay"] = state.show_wrelay;
            doc["show_relay_ext"] = state.show_relay_ext;
            doc["dimmy_pro"] = state.dimmy_pro;
            JsonArray sw_nm = doc["switch_names"].to<JsonArray>();
            JsonArray sw_vis = doc["switch_visible"].to<JsonArray>();
            for (int i = 0; i < 16; i++) {
                sw_nm.add(state.switch_names[i]);
                sw_vis.add(state.switch_visible[i]);
            }
            JsonArray dim_nm = doc["dimmer_names"].to<JsonArray>();
            JsonArray dim_vis = doc["dimmer_visible"].to<JsonArray>();
            for (int i = 0; i < 10; i++) {
                dim_nm.add(state.dimmer_names[i]);
                dim_vis.add(state.dimmer_visible[i]);
            }
            JsonArray wr_nm = doc["wrelay_names"].to<JsonArray>();
            JsonArray wr_vis = doc["wrelay_visible"].to<JsonArray>();
            for (int i = 0; i < 8; i++) {
                wr_nm.add(state.wrelay_names[i]);
                wr_vis.add(state.wrelay_visible[i]);
            }

            doc["outdoor_temp_sensor"] = state.outdoor_temp_sensor;
            doc["indoor_temp_sensor"] = state.indoor_temp_sensor;
            doc["fridge_temp_sensor"] = state.fridge_temp_sensor;
            JsonArray t_cust = doc["temps_custom_sensor"].to<JsonArray>();
            t_cust.add(state.temps_custom_sensor[0]);
            t_cust.add(state.temps_custom_sensor[1]);

            doc["warn_frost_temp"] = state.warn_frost_temp;
            doc["warn_bat_soc"] = state.warn_bat_soc;
            doc["warn_bat_yellow_soc"] = state.warn_bat_yellow_soc;
            doc["warn_fresh_min"] = state.warn_fresh_min;
            doc["warn_waste_max"] = state.warn_waste_max;
            doc["solar_max_w"] = state.solar_max_w;
            doc["shore_max_w"] = state.shore_max_w;
            doc["alt_max_w"] = state.alt_max_w;
            doc["bat_capacity_ah"] = state.bat_capacity_ah;


            doc["home_show_battery"] = state.home_show_battery;
            doc["home_show_starter"] = state.home_show_starter;
            doc["home_show_solar"] = state.home_show_solar;
            doc["home_show_water"] = state.home_show_water;
            victron_fill_settings(doc.as<JsonObject>());
            doc["home_water_source"] = state.home_water_source;
            doc["power_show_solar"] = state.power_show_solar;
            doc["power_show_shore"] = state.power_show_shore;
            doc["power_show_alt"] = state.power_show_alt;
            doc["power_calc_load"] = state.power_calc_load;
            doc["shore_available"] = state.shore_available;
            doc["alt_available"] = state.alt_available;
            JsonArray h_fav = doc["home_favorite"].to<JsonArray>();
            h_fav.add(state.home_favorite[0]);
            h_fav.add(state.home_favorite[1]);

            JsonArray t_ord = doc["tab_order"].to<JsonArray>();
            for (int i = 0; i < TAB_COUNT; i++) t_ord.add(state.tab_order[i]);
            JsonArray t_en = doc["tab_enabled"].to<JsonArray>();
            for (int i = 0; i < TAB_COUNT; i++) t_en.add(state.tab_enabled[i]);

            // Metadata: Relais Options
            JsonArray rel_arr = doc["relays"].to<JsonArray>();
            int max_relays = state.show_relay_ext ? 16 : 8;
            for (int i = 0; i < max_relays; i++) {
                JsonObject r = rel_arr.add<JsonObject>();
                r["id"] = i;
                r["name"] = state.switch_names[i].length() ? state.switch_names[i] : "Relais " + String(i + 1);
                r["is_wrelay"] = false;
            }
            if (state.show_wrelay) {
                for (int i = 0; i < 8; i++) {
                    JsonObject r = rel_arr.add<JsonObject>();
                    r["id"] = 16 + i;
                    r["name"] = state.wrelay_names[i].length() ? state.wrelay_names[i] : "WLAN " + String(i + 1);
                    r["is_wrelay"] = true;
                }
            }

            // Metadata: Dimmer Options (fill assist)
            JsonArray dim_arr = doc["dimmers"].to<JsonArray>();
            int max_dim = state.dimmy_pro ? 10 : 8;
            for (int i = 0; i < max_dim; i++) {
                JsonObject d = dim_arr.add<JsonObject>();
                d["id"] = i;
                String def_nm = (i < 8) ? ("Dimmer " + String(i + 1)) : ("RGBW " + String(i - 7));
                d["name"] = state.dimmer_names[i].length() ? state.dimmer_names[i] : def_nm;
            }

            // Metadata: Temp Sensors
            JsonArray sens_arr = doc["temp_sensors"].to<JsonArray>();
            for (int i = 0; i < TEMP_SOURCE_COUNT; i++) {
                JsonObject s = sens_arr.add<JsonObject>();
                s["idx"] = i;
                String label = (i < 4 ? "Temp " + String(i + 1) : "Ruuvi " + String(i - 4));
                if (state.temp_sensor_names[i].length() > 0 && state.temp_sensor_names[i] != label) {
                    label += " (" + state.temp_sensor_names[i] + ")";
                }
                s["name"] = label;
                s["val"] = state.temp_sensors[i];
                s["hum"] = state.temp_humidity[i];
            }

            // Metadata: Heater Types
            JsonArray ht_arr = doc["heater_types"].to<JsonArray>();
            JsonObject ht0 = ht_arr.add<JsonObject>(); ht0["id"] = 0; ht0["name"] = "Autoterm (Air 2D/4D)";
            JsonObject ht1 = ht_arr.add<JsonObject>(); ht1["id"] = 1; ht1["name"] = "Truma (Gas/Diesel)";
            JsonObject ht2 = ht_arr.add<JsonObject>(); ht2["id"] = 2; ht2["name"] = "Truma + Elektro (Combi E)";
            JsonObject ht4 = ht_arr.add<JsonObject>(); ht4["id"] = 4; ht4["name"] = "Webasto (W-Bus)";
            JsonObject ht3 = ht_arr.add<JsonObject>(); ht3["id"] = 3; ht3["name"] = "Keine / Deaktiviert";
        }
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    server.on("/api/victron", HTTP_GET, [&server]() {
        VictronSnapshot s=victron_snapshot(); JsonDocument doc;
        doc["connected"]=s.connected;doc["demo"]=s.demo;doc["stale"]=s.stale;
        doc["enabled"]=s.enabled;doc["status"]=victron_status(s);doc["portal"]=s.portal;
        doc["starter_ambiguous"]=s.starter_ambiguous;doc["battery_instance"]=s.battery_instance;
        if(victron_fresh(s,GX_STARTER))doc["starter_voltage"]=s.values[GX_STARTER].value;
        doc["instance"]=s.instance;doc["mode_control"]=victron_mode_available(s);
        if(victron_fresh(s,GX_MODE))doc["mode"]=s.values[GX_MODE].value;
        else doc["mode"]=nullptr;
        // Raw control evidence remains inspectable even when writes are disabled.
        JsonObject evidence=doc["vebus_diagnosis"].to<JsonObject>();
        const char *paths[]={"Mode","Connected","ModeIsAdjustable"};
        const GxField fields[]={GX_MODE,GX_DEVICE_CONNECTED,GX_ADJUSTABLE};
        for(int i=0;i<3;++i) {
            const auto &v=s.values[fields[i]];JsonObject item=evidence[paths[i]].to<JsonObject>();
            item["valid"]=v.valid;item["live"]=v.live;
            if(v.valid)item["value"]=v.value;else item["value"]=nullptr;
            if(v.at)item["age_ms"]=millis()-v.at;else item["age_ms"]=nullptr;
            if(s.portal[0]&&s.instance>=0) {
                char topic[160];snprintf(topic,sizeof(topic),"N/%s/vebus/%d/%s",s.portal,s.instance,paths[i]);
                item["topic"]=topic;
            }
        }
        JsonArray relays=doc["relays"].to<JsonArray>();
        for(int i=0;i<2;++i){JsonObject item=relays.add<JsonObject>();item["available"]=victron_relay_available(s,i);
            if(victron_fresh(s,(GxField)(GX_RELAY0+i)))item["state"]=s.values[GX_RELAY0+i].value==1;
            else item["state"]=nullptr;}
        String out;serializeJson(doc,out);server.send(200,"application/json",out);
    });

    // 3. POST /api/settings
    server.on("/api/settings", HTTP_POST, [&server]() {
        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"error\":\"Keine Daten empfangen\"}");
            return;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) {
            server.send(400, "application/json", "{\"error\":\"Ungueltiges JSON\"}");
            return;
        }

        bool wifi_changed = false;
        bool bright_changed = false;
        {
            StateLockGuard lock;
            String gx_error;
            if(doc.containsKey("tab_order")) {
                if(!doc["tab_order"].is<JsonArray>() || doc["tab_order"].size()!=TAB_COUNT)
                    gx_error="Menueformat veraltet: Websettings neu laden";
                else {
                    bool seen[TAB_COUNT]={};
                    for(int i=0;i<TAB_COUNT;++i) {
                        JsonVariant v=doc["tab_order"][i];
                        if(!v.is<int>() || v.as<int>()<0 || v.as<int>()>=TAB_COUNT) {gx_error="Menue-Reihenfolge ungueltig";break;}
                        int id=v.as<int>();if(seen[id]){gx_error="Menue-Reihenfolge enthaelt doppelte Seiten";break;}seen[id]=true;
                    }
                }
            }
            if(doc.containsKey("tab_enabled")) {
                if(!doc["tab_enabled"].is<JsonArray>() || doc["tab_enabled"].size()!=TAB_COUNT)
                    gx_error="Menueformat veraltet: Websettings neu laden";
                else for(int i=0;i<TAB_COUNT;++i)if(!doc["tab_enabled"][i].is<bool>()){gx_error="Menue-Sichtbarkeit: EIN/AUS erwartet";break;}
            }
            if(doc.containsKey("home_show_victron") && !doc["home_show_victron"].is<bool>())gx_error="MultiPlus-Kachel: EIN/AUS erwartet";
            if(doc.containsKey("home_favorite")) {
                if(!doc["home_favorite"].is<JsonArray>() || doc["home_favorite"].size()!=2)gx_error="Zwei Favoriten erwartet";
                else for(int i=0;i<2;++i)if(!doc["home_favorite"][i].is<int>() || doc["home_favorite"][i].as<int>()<0 || doc["home_favorite"][i].as<int>()>18)
                    gx_error="Favorit muss zwischen 0 und 18 liegen";
            }
            if(doc["home_show_water"].as<bool>() && doc["home_show_victron"].as<bool>())gx_error="Home: Wasser oder MultiPlus auswaehlen";
            if(gx_error.length() || !victron_apply_settings(doc.as<JsonVariantConst>(),gx_error)) {
                JsonDocument failure;failure["error"]=gx_error;String out;serializeJson(failure,out);
                server.send(400,"application/json",out);return;
            }
            if (doc["wifi_ssid"].is<const char*>()) {
                String s = doc["wifi_ssid"].as<String>();
                if (s != state.wifi_ssid) { state.wifi_ssid = s; wifi_changed = true; }
            }
            if (doc["wifi_pass"].is<const char*>()) {
                String p = doc["wifi_pass"].as<String>();
                if (p != state.wifi_pass) { state.wifi_pass = p; wifi_changed = true; }
            }
            if (doc["vanpi_ip"].is<const char*>()) state.vanpi_ip = doc["vanpi_ip"].as<String>();

            if (doc["dark_mode"].is<bool>()) state.dark_mode = doc["dark_mode"].as<bool>();
            if (doc["buzzer_enabled"].is<bool>()) state.buzzer_enabled = doc["buzzer_enabled"].as<bool>();
            if (doc["debug_mode"].is<bool>()) state.debug_mode = doc["debug_mode"].as<bool>();
            if (doc["display_brightness"].is<int>()) {
                int b = constrain(doc["display_brightness"].as<int>(), 10, 100);
                if (b != state.display_brightness) { state.display_brightness = b; bright_changed = true; }
            }
            if (doc["display_timeout"].is<int>()) state.display_timeout = doc["display_timeout"].as<int>();
            if (doc["screensaver_enabled"].is<bool>()) state.screensaver_enabled = doc["screensaver_enabled"].as<bool>();
            if (doc["screensaver_brightness"].is<int>()) {
                state.screensaver_brightness = constrain(doc["screensaver_brightness"].as<int>(), 1, 100);
                if (ui_screensaver_is_active()) {
                    uint8_t pwm = ui_screensaver_brightness_to_pwm(state.screensaver_brightness);
                    WS_CH32_IO::setPwm(Wire, pwm);
                }
            }

            if (doc["tank_count"].is<int>()) state.tank_count = constrain(doc["tank_count"].as<int>(), 1, 4);
            if (doc["tank_show_liters"].is<bool>()) state.tank_show_liters = doc["tank_show_liters"].as<bool>();
            if (doc["tank_max"].is<JsonArray>()) {
                JsonArray arr = doc["tank_max"].as<JsonArray>();
                for (int i = 0; i < 4 && i < (int)arr.size(); i++) {
                    int val = arr[i].as<int>();
                    state.tank_max[i] = val > 0 ? val : 100;
                }
            }
            if (doc["tank_unit"].is<JsonArray>()) {
                JsonArray arr = doc["tank_unit"].as<JsonArray>();
                for (int i = 0; i < 4 && i < (int)arr.size(); i++) {
                    state.tank_unit[i] = constrain(arr[i].as<int>(), 0, 1);
                }
            }

            if (doc["pump_relay"].is<int>()) state.pump_relay = doc["pump_relay"].as<int>();
            if (doc["drain_relay"].is<int>()) state.drain_relay = doc["drain_relay"].as<int>();
            if (doc["heater_type"].is<int>()) {
                state.heater_type = constrain(doc["heater_type"].as<int>(), 0, 4);
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
            if (doc["heater_temp_buttons"].is<bool>()) state.heater_temp_buttons = doc["heater_temp_buttons"].as<bool>();

            if (doc["show_wrelay"].is<bool>()) { state.show_wrelay = doc["show_wrelay"].as<bool>(); ui_rebuild_switches(); }
            if (doc["show_relay_ext"].is<bool>()) { state.show_relay_ext = doc["show_relay_ext"].as<bool>(); ui_rebuild_switches(); }
            if (doc["dimmy_pro"].is<bool>()) { state.dimmy_pro = doc["dimmy_pro"].as<bool>(); ui_rebuild_dimmers(); }
            if (doc["switch_visible"].is<JsonArray>()) {
                JsonArray arr = doc["switch_visible"].as<JsonArray>();
                for (int i = 0; i < 16 && i < (int)arr.size(); i++) state.switch_visible[i] = arr[i].as<bool>();
            }
            if (doc["dimmer_visible"].is<JsonArray>()) {
                JsonArray arr = doc["dimmer_visible"].as<JsonArray>();
                for (int i = 0; i < 10 && i < (int)arr.size(); i++) state.dimmer_visible[i] = arr[i].as<bool>();
            }
            if (doc["wrelay_visible"].is<JsonArray>()) {
                JsonArray arr = doc["wrelay_visible"].as<JsonArray>();
                for (int i = 0; i < 8 && i < (int)arr.size(); i++) state.wrelay_visible[i] = arr[i].as<bool>();
            }

            if (doc["outdoor_temp_sensor"].is<int>()) state.outdoor_temp_sensor = doc["outdoor_temp_sensor"].as<int>();
            if (doc["indoor_temp_sensor"].is<int>()) state.indoor_temp_sensor = doc["indoor_temp_sensor"].as<int>();
            if (doc["fridge_temp_sensor"].is<int>()) state.fridge_temp_sensor = doc["fridge_temp_sensor"].as<int>();
            if (doc["temps_custom_sensor"].is<JsonArray>()) {
                JsonArray arr = doc["temps_custom_sensor"].as<JsonArray>();
                if (arr.size() > 0) state.temps_custom_sensor[0] = arr[0].as<int>();
                if (arr.size() > 1) state.temps_custom_sensor[1] = arr[1].as<int>();
            }

            if (doc["warn_frost_temp"].is<float>()) state.warn_frost_temp = doc["warn_frost_temp"].as<float>();
            if (doc["warn_bat_soc"].is<int>()) state.warn_bat_soc = doc["warn_bat_soc"].as<int>();
            if (doc["warn_bat_yellow_soc"].is<int>()) state.warn_bat_yellow_soc = doc["warn_bat_yellow_soc"].as<int>();
            if (doc["warn_fresh_min"].is<int>()) state.warn_fresh_min = doc["warn_fresh_min"].as<int>();
            if (doc["warn_waste_max"].is<int>()) state.warn_waste_max = doc["warn_waste_max"].as<int>();
            if (doc["solar_max_w"].is<float>()) state.solar_max_w = doc["solar_max_w"].as<float>();
            if (doc["shore_max_w"].is<float>()) state.shore_max_w = doc["shore_max_w"].as<float>();
            if (doc["alt_max_w"].is<float>()) state.alt_max_w = doc["alt_max_w"].as<float>();
            if (doc["bat_capacity_ah"].is<float>()) state.bat_capacity_ah = doc["bat_capacity_ah"].as<float>();


            if (doc["home_show_battery"].is<bool>()) state.home_show_battery = doc["home_show_battery"].as<bool>();
            if (doc["home_show_starter"].is<bool>()) state.home_show_starter = doc["home_show_starter"].as<bool>();
            if (doc["home_show_solar"].is<bool>()) state.home_show_solar = doc["home_show_solar"].as<bool>();
            if (doc["home_show_water"].is<bool>()) {
                state.home_show_water = doc["home_show_water"].as<bool>();
                if(state.home_show_water)state.home_show_victron=false;
            }
            if(doc["home_show_victron"].is<bool>()) {
                bool next=doc["home_show_victron"].as<bool>();
                if(next&&!state.home_show_victron)state.tab_enabled[TAB_VICTRON]=true;
                state.home_show_victron=next;if(next)state.home_show_water=false;
            }
            if (doc["home_water_source"].is<int>()) state.home_water_source = doc["home_water_source"].as<int>();
            bool pwr_rebuild = false;
            if (doc["power_show_solar"].is<bool>()) { state.power_show_solar = doc["power_show_solar"].as<bool>(); pwr_rebuild = true; }
            if (doc["power_show_shore"].is<bool>()) { state.power_show_shore = doc["power_show_shore"].as<bool>(); pwr_rebuild = true; }
            if (doc["power_show_alt"].is<bool>()) { state.power_show_alt = doc["power_show_alt"].as<bool>(); pwr_rebuild = true; }
            if (doc["power_calc_load"].is<bool>()) { state.power_calc_load = doc["power_calc_load"].as<bool>(); pwr_rebuild = true; }
            if (pwr_rebuild) ui_rebuild_power();
            if (doc["home_favorite"].is<JsonArray>()) {
                JsonArray arr = doc["home_favorite"].as<JsonArray>();
                if (arr.size() > 0) state.home_favorite[0] = arr[0].as<int>();
                if (arr.size() > 1) state.home_favorite[1] = arr[1].as<int>();
            }

            if (doc["tab_order"].is<JsonArray>()) {
                JsonArray arr = doc["tab_order"].as<JsonArray>();
                for (int i = 0; i < TAB_COUNT && i < (int)arr.size(); i++) state.tab_order[i] = arr[i].as<uint8_t>();
            }
            if (doc["tab_enabled"].is<JsonArray>()) {
                JsonArray arr = doc["tab_enabled"].as<JsonArray>();
                for (int i = 0; i < TAB_COUNT && i < (int)arr.size(); i++) state.tab_enabled[i] = arr[i].as<bool>();
            }
        }

        if (bright_changed) {
            uint8_t pwm = 255 - (state.display_brightness * 255 / 100);
            WS_CH32_IO::setPwm(Wire, pwm);
        }
        state_save();

        if (wifi_changed) {
            WiFi.disconnect();
            wifi_connect_configured();
        }

        ui_init(); // Trigger live async rebuild on display!

        server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Gespeichert\"}");
    });

    // 4. GET /api/wifi/scan
    server.on("/api/wifi/scan", HTTP_GET, [&server]() {
        wifi_prepare_for_scan();
        int n = WiFi.scanNetworks(false, false);
        if (n == WIFI_SCAN_FAILED) {
            WiFi.disconnect(true, false);
            delay(100);
            n = WiFi.scanNetworks(false, false);
        }
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < n; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["ssid"] = WiFi.SSID(i);
            obj["rssi"] = WiFi.RSSI(i);
            obj["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
        WiFi.scanDelete();
        wifi_finish_scan();
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    // 5. POST /api/restart
    server.on("/api/restart", HTTP_POST, [&server]() {
        server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Neustart...\"}");
        lv_timer_t *t = lv_timer_create([](lv_timer_t *timer) {
            lv_timer_del(timer);
            WS_CH32_IO::setPwm(Wire, 255);
            delay(20);
            ESP.restart();
        }, 500, nullptr);
        if (t) lv_timer_set_repeat_count(t, 1);
    });

    // 6. GET /api/theme
    server.on("/api/theme", HTTP_GET, [&server]() {
        JsonDocument doc;
        {
            StateLockGuard lock;
            doc["name"] = state.theme.name;
            char hex[10];
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_primary & 0xffffff));
            doc["color_primary"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_dimmer & 0xffffff));
            doc["color_dimmer"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_relay & 0xffffff));
            doc["color_relay"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_tank & 0xffffff));
            doc["color_tank"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_bg & 0xffffff));
            doc["color_bg"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_card & 0xffffff));
            doc["color_card"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_border & 0xffffff));
            doc["color_border"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_text & 0xffffff));
            doc["color_text"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_muted & 0xffffff));
            doc["color_muted"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_track & 0xffffff));
            doc["color_track"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_btn_text & 0xffffff));
            doc["color_btn_text"] = hex;
            snprintf(hex, sizeof(hex), "#%06lx", (unsigned long)(state.theme.color_btn_text_active & 0xffffff));
            doc["color_btn_text_active"] = hex;

            doc["radius_card"] = state.theme.radius_card;
            doc["radius_button"] = state.theme.radius_button;
            doc["border_width"] = state.theme.border_width;
        }
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    // 7. POST /api/theme
    server.on("/api/theme", HTTP_POST, [&server]() {
        String body = server.hasArg("plain") ? server.arg("plain") : (server.args() > 0 ? server.arg(0) : "");
        if (body.length() == 0) {
            server.send(400, "application/json", "{\"error\":\"Missing body\"}");
            return;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        auto parse_color = [](JsonVariant v, uint32_t def_val) -> uint32_t {
            if (v.is<uint32_t>()) return v.as<uint32_t>();
            if (v.is<const char*>()) {
                const char *s = v.as<const char*>();
                if (*s == '#') s++;
                else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
                char *endp = nullptr;
                unsigned long val = strtoul(s, &endp, 16);
                if (endp != s) return (uint32_t)val;
            }
            return def_val;
        };

        {
            StateLockGuard lock;
            if (doc["name"].is<const char*>()) {
                strncpy(state.theme.name, doc["name"].as<const char*>(), sizeof(state.theme.name) - 1);
                state.theme.name[sizeof(state.theme.name) - 1] = '\0';
            }
            state.theme.color_primary = parse_color(doc["color_primary"], state.theme.color_primary);
            state.theme.color_dimmer  = parse_color(doc["color_dimmer"], state.theme.color_dimmer);
            state.theme.color_relay   = parse_color(doc["color_relay"], state.theme.color_relay);
            state.theme.color_tank    = parse_color(doc["color_tank"], state.theme.color_tank);
            state.theme.color_bg      = parse_color(doc["color_bg"], state.theme.color_bg);
            state.theme.color_card    = parse_color(doc["color_card"], state.theme.color_card);
            state.theme.color_border  = parse_color(doc["color_border"], state.theme.color_border);
            state.theme.color_text    = parse_color(doc["color_text"], state.theme.color_text);
            state.theme.color_muted   = parse_color(doc["color_muted"], state.theme.color_muted);
            state.theme.color_track   = parse_color(doc["color_track"], state.theme.color_track);
            state.theme.color_btn_text = parse_color(doc["color_btn_text"], state.theme.color_btn_text);
            state.theme.color_btn_text_active = parse_color(doc["color_btn_text_active"], state.theme.color_btn_text_active);

            if (doc["radius_card"].is<int>())   state.theme.radius_card = constrain(doc["radius_card"].as<int>(), 0, 30);
            if (doc["radius_button"].is<int>()) state.theme.radius_button = constrain(doc["radius_button"].as<int>(), 0, 20);
            if (doc["border_width"].is<int>())  state.theme.border_width = constrain(doc["border_width"].as<int>(), 0, 3);
            state.theme.version = 3;
        }

        state_save_theme();
        ui_init(); // Trigger live async rebuild on display!

        bool do_restart = !doc["restart"].isNull() && doc["restart"].as<bool>();
        if (do_restart) {
            server.send(200, "application/json", "{\"status\":\"ok\",\"rebooting\":true}");
            lv_timer_t *t = lv_timer_create([](lv_timer_t *timer) {
                lv_timer_del(timer);
                WS_CH32_IO::setPwm(Wire, 255);
                delay(20);
                ESP.restart();
            }, 500, nullptr);
            if (t) lv_timer_set_repeat_count(t, 1);
        } else {
            server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Theme gespeichert!\"}");
        }
    });
}
