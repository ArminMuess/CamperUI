#include "powerunit_settings.h"
#include "powerunit_config.h"
#include "powerunit_client.h"
#include "ui_main.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <stdlib.h>
namespace {
lv_obj_t *screen = nullptr, *form = nullptr, *keyboard = nullptr, *host = nullptr, *port = nullptr, *device = nullptr;
lv_obj_t *notice = nullptr, *buzzer = nullptr, *buzzer_label = nullptr;
void hide_keyboard() { lv_keyboard_set_textarea(keyboard, nullptr); lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); lv_obj_set_height(form, 414); }
void input_event(lv_event_t *e) {
    auto code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(keyboard, lv_event_get_target(e)); lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(form, 220); lv_obj_move_foreground(keyboard); lv_obj_scroll_to_view(lv_event_get_target(e), LV_ANIM_OFF);
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) hide_keyboard();
}
lv_obj_t *text(lv_obj_t *parent, const char *value, int y, const lv_font_t *font = &lv_font_montserrat_14) {
    auto *l = lv_label_create(parent); lv_label_set_text(l, value); lv_obj_set_pos(l, 0, y);
    lv_obj_set_width(l, 430); lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, ui_theme_text(), 0); return l;
}
lv_obj_t *field(const char *caption, const char *value, int y, int limit) {
    text(form, caption, y); auto *a = lv_textarea_create(form); lv_obj_set_pos(a, 0, y + 24); lv_obj_set_size(a, 430, 44);
    lv_textarea_set_one_line(a, true); lv_textarea_set_max_length(a, limit); lv_textarea_set_text(a, value);
    lv_obj_set_style_text_font(a, &lv_font_montserrat_14, 0); lv_obj_add_event_cb(a, input_event, LV_EVENT_ALL, nullptr); return a;
}
void close_settings() {
    if (!screen) return;
    if (lv_scr_act() == screen) lv_scr_load(scr_main);
    lv_obj_del_async(screen); screen = nullptr;
}
void buzzer_text() {
    auto s = powerunit_snapshot();
    const auto &p = s.pending[uint8_t(powerunit::Switch::Buzzer)];
    lv_label_set_text(buzzer_label, !s.fresh ? "Summer der Power Unit: --" : p.active ? "Summer wird umgeschaltet ..." :
                      s.state.switches[uint8_t(powerunit::Switch::Buzzer)] ? "Summer der Power Unit: EIN" : "Summer der Power Unit: AUS");
}
void save(lv_event_t *) {
    auto next = powerunit_endpoint().config;
    snprintf(next.host, sizeof(next.host), "%s", lv_textarea_get_text(host));
    snprintf(next.device_id, sizeof(next.device_id), "%s", lv_textarea_get_text(device));
    const char *v = lv_textarea_get_text(port); char *end = nullptr; long n = strtol(v, &end, 10);
    if (!*v || !end || *end || n < 1 || n > 65535) { lv_label_set_text(notice, "Port muss 1 bis 65535 sein"); return; }
    next.port = uint16_t(n);
    if (!powerunit_config_valid(next)) { lv_label_set_text(notice, "Adresse oder Kennung prüfen"); return; }
    if (!powerunit_config_save(next)) { lv_label_set_text(notice, "Speichern fehlgeschlagen"); return; }
    close_settings();
}
const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>CamperUI Power Unit</title>
<style>body{font:16px system-ui;background:#14171d;color:#f8fafc;max-width:620px;margin:24px auto;padding:16px}label{display:block;margin:16px 0}input{font:inherit;box-sizing:border-box;padding:10px;width:100%}button{font:inherit;padding:12px}small{color:#b8c4d6}</style>
<h1>Pekaway Power Unit</h1><p>Das Display liest die Power Unit über den MQTT-Broker des Pekaway Core (Mosquitto).</p><form id="form">
<label>Broker-IP oder Hostname (leer = VanPi-IP aus den Netzwerk-Einstellungen)<input id="host" maxlength="63"></label><label>MQTT-Port<input id="port" type="number" min="1" max="65535" required></label>
<label>Kennung der Power Unit (leer = automatisch)<input id="device" maxlength="32"></label><button>Speichern</button></form><p id="status" role="status"></p><p><small id="live"></small></p>
<script>const $=id=>document.getElementById(id);
async function load(){try{let r=await fetch('/api/powerunit/settings');if(!r.ok)throw Error('HTTP '+r.status);let c=await r.json();$('host').value=c.host;$('port').value=c.port;$('device').value=c.device_id}catch(e){$('status').textContent='Laden fehlgeschlagen: '+e.message;$('form').querySelector('button').disabled=true}}
async function live(){try{let r=await fetch('/api/powerunit/status');let s=await r.json();$('live').textContent=s.message+(s.device_id?' | Kennung '+s.device_id:'')+(s.fresh?' | Akku '+s.soc+' %':'')}catch(e){}}
$('form').onsubmit=async e=>{e.preventDefault();let b=$('form').querySelector('button');b.disabled=true;try{let body={host:$('host').value.trim(),port:Number($('port').value),device_id:$('device').value.trim()};let r=await fetch('/api/powerunit/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});let c=await r.json();if(!r.ok)throw Error(c.error||'Speichern fehlgeschlagen');$('status').textContent='Gespeichert. Verbindung wird neu aufgebaut.'}catch(e){$('status').textContent=e.message}finally{b.disabled=false}};load();live();setInterval(live,3000);</script></html>)HTML";
}  // namespace

void ui_powerunit_settings_reset() {
    if (screen) { if (lv_scr_act() == screen) lv_scr_load(scr_main); lv_obj_del(screen); }
    screen = form = keyboard = host = port = device = notice = buzzer = buzzer_label = nullptr;
}
void ui_powerunit_open_settings() {
    if (screen) { lv_scr_load(screen); return; }
    auto c = powerunit_endpoint().config; screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, ui_theme_bg(), 0);
    form = lv_obj_create(screen); lv_obj_set_pos(form, 0, 0); lv_obj_set_size(form, 480, 414);
    lv_obj_set_style_bg_color(form, ui_theme_bg(), 0); lv_obj_set_style_pad_all(form, 20, 0); lv_obj_set_scroll_dir(form, LV_DIR_VER);
    text(form, "Power-Unit-Einstellungen", 0, &lv_font_montserrat_18);
    host = field("Broker-IP (leer = VanPi-IP)", c.host, 36, 63);
    char n[8]; snprintf(n, sizeof(n), "%u", c.port); port = field("MQTT-Port", n, 116, 5);
    lv_textarea_set_accepted_chars(port, "0123456789");
    device = field("Kennung (leer = automatisch)", c.device_id, 196, 32);
    auto s = powerunit_snapshot();
    char info[96]; snprintf(info, sizeof(info), "Gefunden: %s", *s.device_id ? s.device_id : "noch keine Power Unit");
    text(form, info, 276, &lv_font_montserrat_12);
    buzzer = lv_btn_create(form); lv_obj_set_pos(buzzer, 0, 304); lv_obj_set_size(buzzer, 430, 44);
    buzzer_label = lv_label_create(buzzer); lv_obj_center(buzzer_label); buzzer_text();
    lv_obj_add_event_cb(buzzer, [](lv_event_t *) {
        auto s = powerunit_snapshot();
        if (s.fresh) powerunit_request(powerunit::Switch::Buzzer, !s.state.switches[uint8_t(powerunit::Switch::Buzzer)]);
        buzzer_text();
    }, LV_EVENT_CLICKED, nullptr);
    notice = lv_label_create(form); lv_obj_set_pos(notice, 0, 360); lv_obj_set_width(notice, 430);
    lv_label_set_text(notice, "Auch im Browser: http://DISPLAY-IP/powerunit");
    lv_obj_set_style_text_font(notice, &lv_font_montserrat_12, 0);
    for (int i = 0; i < 2; ++i) {
        auto *b = lv_btn_create(screen); lv_obj_set_pos(b, 12 + i * 234, 426); lv_obj_set_size(b, 222, 44);
        auto *l = lv_label_create(b); lv_label_set_text(l, i ? "Speichern" : "Zurück"); lv_obj_center(l);
        if (i) lv_obj_add_event_cb(b, save, LV_EVENT_CLICKED, nullptr);
        else lv_obj_add_event_cb(b, [](lv_event_t *) { close_settings(); }, LV_EVENT_CLICKED, nullptr);
    }
    keyboard = lv_keyboard_create(screen); lv_obj_set_size(keyboard, 480, 250); lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_READY || lv_event_get_code(e) == LV_EVENT_CANCEL) hide_keyboard(); }, LV_EVENT_ALL, nullptr);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); lv_scr_load(screen);
}
void powerunit_register_routes(WebServer &server) {
    server.on("/powerunit", HTTP_GET, [&server]() { server.send_P(200, "text/html; charset=utf-8", PAGE); });
    server.on("/api/powerunit/settings", HTTP_GET, [&server]() {
        auto c = powerunit_endpoint().config; JsonDocument d;
        d["host"] = c.host; d["port"] = c.port; d["device_id"] = c.device_id;
        String out; serializeJson(d, out); server.send(200, "application/json", out);
    });
    server.on("/api/powerunit/status", HTTP_GET, [&server]() {
        auto s = powerunit_snapshot(); JsonDocument d;
        d["connected"] = s.connected; d["fresh"] = s.fresh; d["message"] = s.message; d["device_id"] = s.device_id;
        if (s.fresh) {
            d["soc"] = s.state.soc; d["inverter"] = s.state.switches[uint8_t(powerunit::Switch::Inverter)];
            d["ac_power"] = s.state.ac_power; d["battery_power"] = s.state.battery_power;
            d["alerts"] = s.state.alert_count;
        }
        String out; serializeJson(d, out); server.send(200, "application/json", out);
    });
    server.on("/api/powerunit/settings", HTTP_POST, [&server]() {
        if (server.arg("plain").length() > 1024) { server.send(413, "application/json", "{\"error\":\"Zu viele Daten\"}"); return; }
        JsonDocument d; auto err = deserializeJson(d, server.arg("plain"));
        PowerUnitConfig c = powerunit_endpoint().config;
        bool valid = !err && d["host"].is<const char *>() && d["port"].is<int>() && d["port"].as<int>() > 0 &&
                     d["port"].as<int>() <= 65535 && d["host"].as<String>().length() <= 63 &&
                     d["device_id"].is<const char *>() && d["device_id"].as<String>().length() <= 32;
        if (valid) {
            snprintf(c.host, sizeof(c.host), "%s", d["host"].as<const char *>()); c.port = d["port"].as<int>();
            snprintf(c.device_id, sizeof(c.device_id), "%s", d["device_id"].as<const char *>());
        }
        if (!valid || !powerunit_config_valid(c)) { server.send(400, "application/json", "{\"error\":\"Adresse, Port oder Kennung ungueltig\"}"); return; }
        if (!powerunit_config_save(c)) { server.send(500, "application/json", "{\"error\":\"Speichern fehlgeschlagen\"}"); return; }
        server.send(200, "application/json", "{\"saved\":true}");
    });
}
