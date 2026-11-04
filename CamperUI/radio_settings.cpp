#include "radio_settings.h"
#include "radio_config.h"
#include "config/volumio_config.h"
#include "volumio_client.h"
#include "ui_main.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <stdlib.h>
namespace {
lv_obj_t *screen=nullptr,*form=nullptr,*keyboard=nullptr,*host=nullptr,*port=nullptr,*enabled=nullptr,*notice=nullptr;
lv_obj_t *names[3]={},*dab[3]={};
void hide_keyboard(){lv_keyboard_set_textarea(keyboard,nullptr);lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);lv_obj_set_height(form,414);}
void input_event(lv_event_t *e) {
    auto code=lv_event_get_code(e);
    if(code==LV_EVENT_FOCUSED || code==LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(keyboard,lv_event_get_target(e));lv_obj_clear_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(form,220);lv_obj_move_foreground(keyboard);lv_obj_scroll_to_view(lv_event_get_target(e),LV_ANIM_OFF);
    } else if(code==LV_EVENT_READY || code==LV_EVENT_CANCEL)hide_keyboard();
}
void text(lv_obj_t *parent,const char *value,int y) {
    auto *l=lv_label_create(parent);lv_label_set_text(l,value);lv_obj_set_pos(l,0,y);
    lv_obj_set_width(l,430);lv_obj_set_style_text_font(l,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(l,ui_theme_text(),0);
}
lv_obj_t *field(const char *caption,const char *value,int y,int limit) {
    text(form,caption,y);auto *a=lv_textarea_create(form);lv_obj_set_pos(a,0,y+24);lv_obj_set_size(a,430,44);
    lv_textarea_set_one_line(a,true);lv_textarea_set_max_length(a,limit);lv_textarea_set_text(a,value);
    lv_obj_set_style_text_font(a,&lv_font_montserrat_14,0);lv_obj_add_event_cb(a,input_event,LV_EVENT_ALL,nullptr);return a;
}
void close_settings() {
    if(!screen)return;
    if(lv_scr_act()==screen)lv_scr_load(scr_main);
    lv_obj_del_async(screen);screen=nullptr;
}
void save(lv_event_t*) {
    auto next=radio_endpoint().config;
    next.enabled=lv_obj_has_state(enabled,LV_STATE_CHECKED);
    snprintf(next.host,sizeof(next.host),"%s",lv_textarea_get_text(host));
    const char *v=lv_textarea_get_text(port);char *end=nullptr;long n=strtol(v,&end,10);
    if(!*v || !end || *end || n<1 || n>65535){lv_label_set_text(notice,"Port muss 1 bis 65535 sein");return;}
    next.port=uint16_t(n);
    for(int i=0;i<3;++i){snprintf(next.playlists[i],sizeof(next.playlists[i]),"%s",lv_textarea_get_text(names[i]));
        snprintf(next.dab_uris[i],sizeof(next.dab_uris[i]),"%s",lv_textarea_get_text(dab[i]));}
    if(!radio_config_valid(next)){lv_label_set_text(notice,"Host, Playlistnamen oder DAB-URI prüfen");return;}
    if(!radio_config_save(next)){lv_label_set_text(notice,"Speichern fehlgeschlagen");return;}
    close_settings();
}
const char PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>CamperUI Radio</title>
<style>body{font:16px system-ui;background:#14171d;color:#f8fafc;max-width:620px;margin:24px auto;padding:16px}label{display:block;margin:16px 0}input{font:inherit;box-sizing:border-box;padding:10px;width:100%}input[type=checkbox]{width:auto}button{font:inherit;padding:12px}small{color:#b8c4d6}</style>
<h1>Radio / Volumio</h1><p>Lokale Verbindung; ohne Adresse bleibt Radio offline.</p><form id="form">
<label><input type="checkbox" id="enabled"> Radio aktiv</label><label>Volumio-IP oder Hostname (ohne http://)<input id="host" maxlength="63"></label><label>HTTP-Port<input id="port" type="number" min="1" max="65535" required></label>
<div id="presets"></div><button>Speichern</button></form><p id="status" role="status"></p><p><small>DAB-URIs aus Volumio uebernehmen; keinen Sendersuchlauf vom Panel starten. Spotify wird am Handy oder in Volumio ausgewaehlt.</small></p>
<script>const $=id=>document.getElementById(id);const captions=['hr3','RADIO BOB!','ROCK ANTENNE'];
for(let i=0;i<3;i++){for(const [key,label,max] of [['playlist','Internet-Playlist',63],['dab','DAB-URI (optional)',255]]){const l=document.createElement('label');l.textContent=captions[i]+': '+label;const a=document.createElement('input');a.id=key+i;a.maxLength=max;l.append(a);$('presets').append(l)}}
async function load(){try{let r=await fetch('/api/radio/settings');if(!r.ok)throw Error('HTTP '+r.status);let c=await r.json();$('enabled').checked=c.enabled;$('host').value=c.host;$('port').value=c.port;for(let i=0;i<3;i++){$('playlist'+i).value=c.playlists[i];$('dab'+i).value=c.dab_uris[i]}}catch(e){$('status').textContent='Laden fehlgeschlagen: '+e.message;$('form').querySelector('button').disabled=true}}
$('form').onsubmit=async e=>{e.preventDefault();let b=$('form').querySelector('button');b.disabled=true;try{let body={enabled:$('enabled').checked,host:$('host').value.trim(),port:Number($('port').value),playlists:captions.map((_,i)=>$('playlist'+i).value),dab_uris:captions.map((_,i)=>$('dab'+i).value)};let r=await fetch('/api/radio/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});let c=await r.json();if(!r.ok)throw Error(c.error||'Speichern fehlgeschlagen');$('status').textContent='Gespeichert. Verbindung wird neu geprueft.'}catch(e){$('status').textContent=e.message}finally{b.disabled=false}};load();</script></html>)HTML";
void fill(JsonDocument &d,const RadioConfig &c) {
    d["enabled"]=c.enabled;d["host"]=c.host;d["port"]=c.port;
    for(int i=0;i<3;++i){d["playlists"][i]=c.playlists[i];d["dab_uris"][i]=c.dab_uris[i];}
}
}
void ui_radio_settings_reset() {
    if(screen){if(lv_scr_act()==screen)lv_scr_load(scr_main);lv_obj_del(screen);}
    screen=form=keyboard=host=port=enabled=notice=nullptr;
    for(int i=0;i<3;++i)names[i]=dab[i]=nullptr;
}
void ui_radio_open_settings() {
    if(screen){lv_scr_load(screen);return;}
    auto c=radio_endpoint().config;screen=lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen,ui_theme_bg(),0);
    form=lv_obj_create(screen);lv_obj_set_pos(form,0,0);lv_obj_set_size(form,480,414);
    lv_obj_set_style_bg_color(form,ui_theme_bg(),0);lv_obj_set_style_pad_all(form,20,0);lv_obj_set_scroll_dir(form,LV_DIR_VER);
    text(form,"Radio-Einstellungen",0);enabled=lv_switch_create(form);lv_obj_set_pos(enabled,0,32);
    if(c.enabled)lv_obj_add_state(enabled,LV_STATE_CHECKED);text(form,"Radio aktiv",62);
    host=field("Volumio-IP oder Hostname",c.host,92,63);
    char n[8];snprintf(n,sizeof(n),"%u",c.port);port=field("HTTP-Port",n,172,5);lv_textarea_set_accepted_chars(port,"0123456789");
    for(int i=0;i<3;++i){char caption[64];snprintf(caption,sizeof(caption),"%s: Internet-Playlist",radio_config::PRESET_LABELS[i]);
        names[i]=field(caption,c.playlists[i],252+i*168,63);
        snprintf(caption,sizeof(caption),"%s: DAB-URI (optional)",radio_config::PRESET_LABELS[i]);
        dab[i]=field(caption,c.dab_uris[i],332+i*168,255);}
    notice=lv_label_create(form);lv_obj_set_pos(notice,0,766);lv_obj_set_width(notice,430);
    lv_label_set_text(notice,"DAB-URIs später bequem im Browser:\nhttp://DISPLAY-IP/radio");
    lv_obj_set_style_text_font(notice,&lv_font_montserrat_12,0);
    for(int i=0;i<2;++i){auto *b=lv_btn_create(screen);lv_obj_set_pos(b,12+i*234,426);lv_obj_set_size(b,222,44);
        auto *l=lv_label_create(b);lv_label_set_text(l,i?"Speichern":"Zurück");lv_obj_center(l);
        if(i)lv_obj_add_event_cb(b,save,LV_EVENT_CLICKED,nullptr);
        else lv_obj_add_event_cb(b,[](lv_event_t*){close_settings();},LV_EVENT_CLICKED,nullptr);}
    keyboard=lv_keyboard_create(screen);lv_obj_set_size(keyboard,480,250);lv_obj_align(keyboard,LV_ALIGN_BOTTOM_MID,0,0);
    lv_obj_add_event_cb(keyboard,[](lv_event_t *e){if(lv_event_get_code(e)==LV_EVENT_READY||lv_event_get_code(e)==LV_EVENT_CANCEL)hide_keyboard();},LV_EVENT_ALL,nullptr);
    lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);lv_scr_load(screen);
}
void radio_register_routes(WebServer &server) {
    server.on("/radio",HTTP_GET,[&server](){server.send_P(200,"text/html; charset=utf-8",PAGE);});
    server.on("/api/radio/settings",HTTP_GET,[&server](){JsonDocument d;fill(d,radio_endpoint().config);String out;serializeJson(d,out);server.send(200,"application/json",out);});
    server.on("/api/radio/status",HTTP_GET,[&server](){auto s=radio_snapshot();JsonDocument d;
        d["online"]=s.online;d["pending"]=s.pending;d["status"]=s.playback;d["service"]=s.service;d["uri"]=s.uri;
        d["message"]=s.message;d["volume_available"]=s.volume_available;if(s.online&&s.volume_available)d["volume"]=s.volume;
        d["artist"]=s.artist;d["title"]=s.title;String out;serializeJson(d,out);server.send(200,"application/json",out);});
    server.on("/api/radio/settings",HTTP_POST,[&server](){
        if(server.arg("plain").length()>4096){server.send(413,"application/json","{\"error\":\"Zu viele Daten\"}");return;}
        JsonDocument d;auto err=deserializeJson(d,server.arg("plain"));
        RadioConfig c;bool valid=!err && d["enabled"].is<bool>() && d["host"].is<const char*>() && d["port"].is<int>() && d["port"].as<int>()>0 && d["port"].as<int>()<=65535 && d["host"].as<String>().length()<=63 && d["playlists"].is<JsonArray>() && d["playlists"].size()==3 && d["dab_uris"].is<JsonArray>() && d["dab_uris"].size()==3;
        if(valid){c.enabled=d["enabled"].as<bool>();snprintf(c.host,sizeof(c.host),"%s",d["host"].as<const char*>());c.port=d["port"].as<int>();
            for(int i=0;i<3;++i){if(!d["playlists"][i].is<const char*>() || !d["dab_uris"][i].is<const char*>() || d["playlists"][i].as<String>().length()>63 || d["dab_uris"][i].as<String>().length()>255){valid=false;break;}
                snprintf(c.playlists[i],sizeof(c.playlists[i]),"%s",d["playlists"][i].as<const char*>());snprintf(c.dab_uris[i],sizeof(c.dab_uris[i]),"%s",d["dab_uris"][i].as<const char*>());}}
        if(!valid || !radio_config_valid(c)){server.send(400,"application/json","{\"error\":\"Adresse, Port oder Presets ungueltig\"}");return;}
        if(!radio_config_save(c)){server.send(500,"application/json","{\"error\":\"Speichern fehlgeschlagen\"}");return;}
        server.send(200,"application/json","{\"saved\":true}");
    });
}
