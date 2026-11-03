#include "ui_radio.h"
#include "radio_settings.h"
#include "radio_config.h"
#include "ui_main.h"
#include "volumio_client.h"
#include "config/volumio_config.h"
namespace {
lv_obj_t *root, *status, *artist, *title, *playback, *volume_label, *slider;
lv_obj_t *presets[3], *preset_labels[3], *banks[3], *play_button, *stop_button;
int bank=0; // Selection changes the controls only; it never starts a source.
lv_timer_t *refresh = nullptr;
bool dragging = false;
lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width,
                const lv_font_t *font = &lv_font_montserrat_16) {
    auto *l = lv_label_create(parent); lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y); lv_obj_set_width(l, width);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, ui_theme_text(), 0); return l;
}
void enable(lv_obj_t *obj, bool yes) {
    if (yes) lv_obj_clear_state(obj, LV_STATE_DISABLED); else lv_obj_add_state(obj, LV_STATE_DISABLED);
}
void click(lv_event_t *e) {
    auto id = intptr_t(lv_event_get_user_data(e));
    if(id>=10 && id<13){bank=int(id)-10;ui_update_radio();return;}
    if(id==20){ui_radio_open_settings();return;}
    if(id<3) {
        if(bank==2)radio_request(id==0?radio_protocol::Action::Previous:id==1?radio_protocol::Action::Pause:radio_protocol::Action::Next);
        else radio_request(bank==1?radio_protocol::Action::DabPreset:radio_protocol::Action::Preset,int(id));
    } else radio_request(id==3?radio_protocol::Action::Play:radio_protocol::Action::Stop);
    ui_update_radio();
}
lv_obj_t *button(const char *text, int x, int y, int width, intptr_t id) {
    auto *b = lv_btn_create(root); lv_obj_set_pos(b, x, y); lv_obj_set_size(b, width, 48);
    lv_obj_set_style_radius(b, ui_radius_button(), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_bg_color(b, ui_theme_card(), 0);
    lv_obj_set_style_border_width(b, ui_border_width(), 0);
    lv_obj_set_style_border_color(b, ui_theme_border(), 0);
    auto *l = lv_label_create(b); lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, ui_theme_btn_text(), 0); lv_obj_center(l);
    lv_obj_add_event_cb(b, click, LV_EVENT_CLICKED, reinterpret_cast<void *>(id)); return b;
}
void volume_event(lv_event_t *e) {
    auto code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) dragging = true;
    if (code == LV_EVENT_VALUE_CHANGED) {
        char line[48]; snprintf(line, sizeof(line), "Lautstaerke: %d %%", int(lv_slider_get_value(slider)));
        ui_label_set_text_if_changed(volume_label, line);
    }
    if (code == LV_EVENT_RELEASED) {
        int value = lv_slider_get_value(slider); dragging = false;
        radio_request(radio_protocol::Action::Volume, value); ui_update_radio();
    }
    if (code == LV_EVENT_PRESS_LOST) { dragging = false; ui_update_radio(); }
}
}
void ui_radio_reset() {
    if (refresh) { lv_timer_del(refresh); refresh = nullptr; }
    ui_radio_settings_reset();
    root = status = artist = title = playback = volume_label = slider = nullptr;
    play_button = stop_button = nullptr; for (auto &p : presets) p = nullptr;
    for(auto &p: banks)p=nullptr;for(auto &p:preset_labels)p=nullptr; dragging = false;
}
void ui_build_radio(lv_obj_t *parent) {
    root = parent; ui_setup_tab_page(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    label(root, "Radio / Volumio", 0, 0, 384, &lv_font_montserrat_20);
    auto *gear=button("",396,0,44,20);lv_obj_set_height(gear,28);
    auto *icon=lv_label_create(gear);lv_label_set_text(icon,MDI_COG);
    lv_obj_set_style_text_font(icon,&ui_font_mdi_24,0);lv_obj_center(icon);
    status = label(root, "--", 0, 29, 440, &lv_font_montserrat_12);
    const char *sources[]={"Internet","DAB+","Spotify"};
    for(int i=0;i<3;++i){banks[i]=button(sources[i],i*149,49,142,10+i);lv_obj_set_height(banks[i],32);}
    for(int i=0;i<3;++i){presets[i]=button(radio_config::PRESET_LABELS[i],i*149,91,142,i);
        preset_labels[i]=lv_obj_get_child(presets[i],0);}
    artist = label(root, "--", 0, 149, 440, &lv_font_montserrat_20);
    title = label(root, "--", 0, 179, 440);
    playback = label(root, "--", 0, 205, 440, &lv_font_montserrat_12);
    play_button = button("Start", 0, 232, 216, 3);
    stop_button = button("Stopp", 224, 232, 216, 4);
    volume_label = label(root, "Lautstaerke: --", 0, 292, 440);
    slider = lv_slider_create(root); lv_obj_set_pos(slider, 14, 335); lv_obj_set_size(slider, 412, 18);
    lv_slider_set_range(slider, 0, 100);
    lv_obj_set_style_bg_color(slider, ui_theme_track(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, ui_theme_primary(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, ui_theme_text(), LV_PART_KNOB);
    lv_obj_add_event_cb(slider, volume_event, LV_EVENT_ALL, nullptr);
    refresh = lv_timer_create([](lv_timer_t *) {
        if (root && lv_scr_act()==scr_main && !lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) ui_update_radio();
    }, 200, nullptr);
    ui_update_radio();
}
void ui_update_radio() {
    if (!root) return;
    auto s = radio_snapshot();
    ui_label_set_text_if_changed(status, s.pending ? "Befehl wird gesendet..." : s.message);
    ui_label_set_text_if_changed(artist, s.online ? (*s.artist ? s.artist : "Volumio") : "Radio nicht verbunden");
    ui_label_set_text_if_changed(title, s.online ? (*s.title ? s.title : "Keine Titelinformation") : "--");
    bool spotify=radio_protocol::source(s.service,s.uri)==radio_protocol::Source::Spotify;
    bool dab_active=radio_protocol::source(s.service,s.uri)==radio_protocol::Source::Dab;
    char line[96];
    snprintf(line,sizeof(line),"%s | %s",!s.online?"Offline":spotify?"Spotify":dab_active?"DAB+":"Volumio",
        !s.online?"Status unbekannt":!strcmp(s.playback,"play")?"Wiedergabe":!strcmp(s.playback,"pause")?"Pausiert":"Gestoppt");
    ui_label_set_text_if_changed(playback,line);
    bool ready=s.online && !s.pending;
    auto config=radio_endpoint().config;
    const char *spotify_labels[]={"Zurueck","Pause","Weiter"};
    for(int i=0;i<3;++i) {
        ui_bg_color_if_changed(banks[i],i==bank?ui_theme_primary():ui_theme_card());
        ui_label_set_text_if_changed(preset_labels[i],bank==2?spotify_labels[i]:radio_config::PRESET_LABELS[i]);
        enable(presets[i],ready && (bank==0 || (bank==1 && *config.dab_uris[i]) || (bank==2 && spotify)));
    }
    if(bank==2 && s.online && !spotify)
        ui_label_set_text_if_changed(status,"Spotify zuerst am Handy / in Volumio auswaehlen");
    else if(bank==1 && !*config.dab_uris[0] && !*config.dab_uris[1] && !*config.dab_uris[2])
        ui_label_set_text_if_changed(status,"DAB-Presets unter Zahnrad einrichten");
    enable(play_button,ready && (bank!=2 || spotify));enable(stop_button,ready);
    if (!dragging) {
        enable(slider, ready && s.volume_available);
        if (s.online && s.volume_available) {
            char line[48]; snprintf(line, sizeof(line), "Lautstaerke: %d %% %s", s.volume, s.muted ? "(stumm)" : "");
            ui_label_set_text_if_changed(volume_label, line);
            lv_slider_set_value(slider, s.volume, LV_ANIM_OFF);
        } else ui_label_set_text_if_changed(volume_label, "Lautstaerke: --");
    }
}
