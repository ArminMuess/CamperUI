#include "ui_radio.h"
#include "radio_settings.h"
#include "radio_config.h"
#include "ui_main.h"
#include "volumio_client.h"
#include "config/volumio_config.h"
#include "ui_screensaver.h"
namespace {
lv_obj_t *root, *status, *artist, *title, *playback, *volume_label, *slider;
lv_obj_t *presets[3], *preset_labels[3], *banks[3], *play_button, *stop_button;
lv_obj_t *power_button = nullptr, *power_icon = nullptr;
uint32_t power_hint_at = 0;   // tick of the "hold to switch off" hint (0 = none)
int bank=0; // Selection changes the controls only; it never starts a source.
lv_timer_t *refresh = nullptr;
bool dragging = false;
// Internet presets start a Volumio playlist; the stream URI it plays is learned from
// the player state after a tap, so the running preset can be marked afterwards.
char learned_uri[3][256] = {};
int learning = -1;               // preset waiting for its URI
uint32_t learning_since = 0;
char uri_before[256] = "";
bool same(const char *a, const char *b) { return *a && *b && !strcmp(a, b); }
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
        else if(radio_request(bank==1?radio_protocol::Action::DabPreset:radio_protocol::Action::Preset,int(id)) && bank==0) {
            auto s=radio_snapshot();
            learning=int(id);learning_since=lv_tick_get();
            snprintf(uri_before,sizeof(uri_before),"%s",s.uri);
        }
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
// Power: tap switches on; switching off needs a long press so it cannot happen by accident.
void power_event(lv_event_t *e) {
    auto code = lv_event_get_code(e);
    auto s = radio_snapshot();
    bool running = s.power == RadioPower::On || s.power == RadioPower::Starting;
    if (code == LV_EVENT_SHORT_CLICKED) {
        if (s.power == RadioPower::Off || s.power == RadioPower::Unknown) radio_power_request(true);
        else if (running) power_hint_at = lv_tick_get() | 1;
    } else if (code == LV_EVENT_LONG_PRESSED && s.power == RadioPower::On) {
        power_hint_at = 0;   // the shutdown message replaces the hint at once
        radio_power_request(false);
    }
    ui_update_radio();
}
void volume_event(lv_event_t *e) {
    auto code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) dragging = true;
    if (code == LV_EVENT_VALUE_CHANGED) {
        char line[48]; snprintf(line, sizeof(line), "Lautstärke: %d %%", int(lv_slider_get_value(slider)));
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
    power_button = power_icon = nullptr; power_hint_at = 0;
    for(auto &p: banks)p=nullptr;for(auto &p:preset_labels)p=nullptr; dragging = false;
    learning = -1;
    radio_set_poll_mode(RadioPollMode::Off);
}
void ui_build_radio(lv_obj_t *parent) {
    root = parent; ui_setup_tab_page(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    label(root, "Radio / Volumio", 0, 0, 384, &lv_font_montserrat_20);
    power_button=lv_btn_create(root);lv_obj_set_pos(power_button,344,0);lv_obj_set_size(power_button,44,28);
    lv_obj_set_style_radius(power_button,ui_radius_button(),0);lv_obj_set_style_shadow_width(power_button,0,0);
    lv_obj_set_style_border_width(power_button,ui_border_width(),0);lv_obj_set_style_border_color(power_button,ui_theme_border(),0);
    power_icon=lv_label_create(power_button);lv_label_set_text(power_icon,LV_SYMBOL_POWER);lv_obj_center(power_icon);
    lv_obj_set_style_text_font(power_icon,&lv_font_montserrat_16,0);
    lv_obj_add_event_cb(power_button,power_event,LV_EVENT_SHORT_CLICKED,nullptr);
    lv_obj_add_event_cb(power_button,power_event,LV_EVENT_LONG_PRESSED,nullptr);
    lv_obj_add_flag(power_button,LV_OBJ_FLAG_HIDDEN);
    auto *gear=button("",396,0,44,20);lv_obj_set_height(gear,28);
    auto *icon=lv_label_create(gear);lv_label_set_text(icon,MDI_COG);
    lv_obj_set_style_text_font(icon,&ui_font_mdi_24,0);lv_obj_center(icon);
    status = label(root, "--", 0, 29, 440, &lv_font_montserrat_12);
    const char *sources[]={"Internet","DAB+","Spotify"};
    for(int i=0;i<3;++i){banks[i]=button(sources[i],i*149,49,142,10+i);lv_obj_set_height(banks[i],32);}
    for(int i=0;i<3;++i){presets[i]=button(radio_config::PRESET_LABELS[i],i*149,91,142,i);
        preset_labels[i]=lv_obj_get_child(presets[i],0);
        lv_obj_set_width(preset_labels[i],130);lv_label_set_long_mode(preset_labels[i],LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(preset_labels[i],LV_TEXT_ALIGN_CENTER,0);lv_obj_center(preset_labels[i]);}
    artist = label(root, "--", 0, 149, 440, &lv_font_montserrat_20);
    title = label(root, "--", 0, 179, 440);
    playback = label(root, "--", 0, 205, 440, &lv_font_montserrat_12);
    play_button = button("Start", 0, 232, 216, 3);
    stop_button = button("Stopp", 224, 232, 216, 4);
    volume_label = label(root, "Lautstärke: --", 0, 292, 440);
    slider = lv_slider_create(root); lv_obj_set_pos(slider, 14, 335); lv_obj_set_size(slider, 412, 18);
    lv_slider_set_range(slider, 0, 100);
    lv_obj_set_style_bg_color(slider, ui_theme_track(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, ui_theme_primary(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, ui_theme_text(), LV_PART_KNOB);
    lv_obj_add_event_cb(slider, volume_event, LV_EVENT_ALL, nullptr);
    refresh = lv_timer_create([](lv_timer_t *) {
        bool display = display_is_on && !ui_screensaver_is_active();
        bool visible = display && root && lv_scr_act()==scr_main && !lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN);
        // Fast polling only while someone can see the page; none while the display is dark.
        radio_set_poll_mode(visible ? RadioPollMode::Visible : display ? RadioPollMode::Background : RadioPollMode::Off);
        if (visible) ui_update_radio();
    }, 200, nullptr);
    ui_update_radio();
}
void ui_update_radio() {
    if (!root) return;
    auto s = radio_snapshot();
    // Power button and power messages (only when a Shelly is configured)
    bool has_power = s.power != RadioPower::None;
    bool powering = s.power == RadioPower::Starting || s.power == RadioPower::Stopping;
    if (has_power == lv_obj_has_flag(power_button, LV_OBJ_FLAG_HIDDEN)) {
        if (has_power) lv_obj_clear_flag(power_button, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(power_button, LV_OBJ_FLAG_HIDDEN);
    }
    lv_color_t pc = s.power == RadioPower::On ? ui_theme_relay() : powering ? lv_color_hex(UI_COLOR_WARNING) : ui_theme_card();
    ui_bg_color_if_changed(power_button, pc);
    ui_text_color_if_changed(power_icon, s.power == RadioPower::On || powering ? ui_theme_btn_active_text() : ui_theme_btn_text());
    const char *status_text = s.pending ? "Befehl wird gesendet..." : s.message;
    if (has_power) {
        if (power_hint_at && lv_tick_elaps(power_hint_at) < 3000) status_text = "Zum Ausschalten den Ein/Aus-Knopf gedrückt halten";
        else if (*s.power_message && (powering || uint32_t(millis() - s.power_message_at) < 8000))
            status_text = s.power_message;
        else if (s.power == RadioPower::Off) status_text = "Radio-Pi ist aus - Einschalten mit dem Ein/Aus-Knopf";
    }
    ui_label_set_text_if_changed(status, status_text);
    const char *offline = s.power == RadioPower::Off ? "Radio-Pi ausgeschaltet" : powering ? "Bitte warten ..." : "Radio nicht verbunden";
    ui_label_set_text_if_changed(artist, s.online ? (*s.artist ? s.artist : "Volumio") : offline);
    ui_label_set_text_if_changed(title, s.online ? (*s.title ? s.title : "Keine Titelinformation") : "--");
    bool spotify=radio_protocol::source(s.service,s.uri)==radio_protocol::Source::Spotify;
    bool dab_active=radio_protocol::source(s.service,s.uri)==radio_protocol::Source::Dab;
    char line[96];
    snprintf(line,sizeof(line),"%s | %s",!s.online?"Offline":spotify?"Spotify":dab_active?"DAB+":"Volumio",
        !s.online?"Status unbekannt":!strcmp(s.playback,"play")?"Wiedergabe":!strcmp(s.playback,"pause")?"Pausiert":"Gestoppt");
    ui_label_set_text_if_changed(playback,line);
    bool ready=s.online && !s.pending;
    auto config=radio_endpoint().config;
    const char *spotify_labels[]={"Zurück","Pause","Weiter"};
    bool playing=s.online && !strcmp(s.playback,"play");
    // Learn which stream an Internet preset plays: the first new URI after the tap.
    if(learning>=0) {
        if(lv_tick_elaps(learning_since)>15000) learning=-1;
        else if(playing && *s.uri && strcmp(s.uri,uri_before) && !dab_active && !spotify) {
            for(auto &u:learned_uri) if(!strcmp(u,s.uri)) u[0]=0;   // one preset per stream
            snprintf(learned_uri[learning],sizeof(learned_uri[learning]),"%s",s.uri);
            learning=-1;
        }
    }
    for(int i=0;i<3;++i) {
        ui_bg_color_if_changed(banks[i],i==bank?ui_theme_primary():ui_theme_card());
        char name[64];
        const char *text=radio_config::PRESET_LABELS[i];
        if(bank==2) text=spotify_labels[i];
        else if(bank==1) {
            if(!radio_protocol::dab_name(config.dab_uris[i],name,sizeof(name))) snprintf(name,sizeof(name),"DAB %d",i+1);
            text=name;
        }
        ui_label_set_text_if_changed(preset_labels[i],text);
        enable(presets[i],ready && (bank==0 || (bank==1 && *config.dab_uris[i]) || (bank==2 && spotify)));
        bool active=playing && ((bank==1 && same(s.uri,config.dab_uris[i])) ||
                                (bank==0 && !dab_active && !spotify && same(s.uri,learned_uri[i])));
        ui_bg_color_if_changed(presets[i],active?ui_theme_relay():ui_theme_card());
        ui_border_color_if_changed(presets[i],active?ui_theme_relay():ui_theme_border());
        ui_text_color_if_changed(preset_labels[i],active?ui_theme_btn_active_text():ui_theme_btn_text());
    }
    if(bank==2 && s.online && !spotify)
        ui_label_set_text_if_changed(status,"Spotify zuerst am Handy / in Volumio auswählen");
    else if(bank==1 && !*config.dab_uris[0] && !*config.dab_uris[1] && !*config.dab_uris[2])
        ui_label_set_text_if_changed(status,"DAB-Presets unter Zahnrad einrichten");
    enable(play_button,ready && (bank!=2 || spotify));enable(stop_button,ready);
    if (!dragging) {
        enable(slider, ready && s.volume_available);
        if (s.online && s.volume_available) {
            char line[48]; snprintf(line, sizeof(line), "Lautstärke: %d %% %s", s.volume, s.muted ? "(stumm)" : "");
            ui_label_set_text_if_changed(volume_label, line);
            lv_slider_set_value(slider, s.volume, LV_ANIM_OFF);
        } else ui_label_set_text_if_changed(volume_label, "Lautstärke: --");
    }
}
