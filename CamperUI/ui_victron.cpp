#include "ui_victron.h"
#include "victron_client.h"
#include "ui_control_style.h"
#include <math.h>

namespace {
struct ModeWidgets {lv_obj_t *buttons[3]={},*labels[3]={};};
lv_obj_t *root,*heading,*status,*ac_in,*ac_out,*dc,*extra,*relay_names[2],*relay_buttons[2],*relay_labels[2];
lv_obj_t *home_root,*home_title,*home_power_root,*home_power_labels[3],*home_power_values[3];
ModeWidgets detail_modes,home_modes;
bool off_feedback_active=false;
uint32_t off_feedback_started=0,off_feedback_session=0;
constexpr uint32_t OFF_FEEDBACK_MS=10000;
const int modes[3]={3,4,1};const char *captions[3]={"EIN","AUS","LADEN"};
lv_obj_t *label(lv_obj_t *parent,const char *value,int x,int y,const lv_font_t *font) {
    lv_obj_t *l=lv_label_create(parent);lv_label_set_text(l,value);lv_obj_set_pos(l,x,y);
    lv_obj_set_style_text_font(l,font,0);lv_obj_set_style_text_color(l,ui_theme_text(),0);return l;
}
void set_button(lv_obj_t *b,lv_obj_t *l,bool checked,bool ready) {
    if(checked)lv_obj_add_state(b,LV_STATE_CHECKED);else lv_obj_clear_state(b,LV_STATE_CHECKED);
    if(ready)lv_obj_clear_state(b,LV_STATE_DISABLED);else lv_obj_add_state(b,LV_STATE_DISABLED);
    ui_text_color_if_changed(l,checked?lv_color_hex(0xffffff):ready?ui_theme_text():ui_theme_muted());
}
void mode_click(lv_event_t *event) {
    int mode=(int)(intptr_t)lv_event_get_user_data(event);
    if(victron_request_mode(mode)) {
        off_feedback_active=mode==4;off_feedback_started=millis();
        off_feedback_session=victron_snapshot().session;
    }
    ui_update_victron();ui_update_victron_home();
}
void build_modes(lv_obj_t *parent,ModeWidgets &w,int x,int y,int width) {
    int button_width=(width-16)/3;
    for(int i=0;i<3;++i){w.buttons[i]=lv_btn_create(parent);lv_obj_set_pos(w.buttons[i],x+i*(button_width+8),y);
        lv_obj_set_width(w.buttons[i],button_width);ui_favorite_button_style(w.buttons[i]);
        lv_obj_add_event_cb(w.buttons[i],mode_click,LV_EVENT_CLICKED,(void*)(intptr_t)modes[i]);
        w.labels[i]=label(w.buttons[i],captions[i],0,0,&lv_font_montserrat_16);lv_obj_center(w.labels[i]);}
}
void update_modes(ModeWidgets &w,const VictronSnapshot &s) {
    bool ready=victron_mode_available(s);
    if(off_feedback_active && (s.session!=off_feedback_session ||
        millis()-off_feedback_started>=OFF_FEEDBACK_MS || s.pending[0].phase==GxCommandPhase::Failed ||
        s.pending[0].phase==GxCommandPhase::Timeout))off_feedback_active=false;
    for(int i=0;i<3;++i)if(w.buttons[i])set_button(w.buttons[i],w.labels[i],
        victron_fresh(s,GX_MODE)&&s.values[GX_MODE].value==modes[i] &&
        (modes[i]!=4 || off_feedback_active),ready);
}
void formatted(char *out,size_t size,const VictronSnapshot &s,GxField field,const char *format) {
    if(victron_fresh(s,field))snprintf(out,size,format,s.values[field].value);else snprintf(out,size,"--");
}
GxField input_power(const VictronSnapshot &s){return victron_fresh(s,GX_IN_TOTAL)?GX_IN_TOTAL:GX_IN_P;}
GxField output_power(const VictronSnapshot &s){return victron_fresh(s,GX_OUT_TOTAL)?GX_OUT_TOTAL:GX_OUT_P;}
void relay_click(lv_event_t *event) {
    int relay=(int)(intptr_t)lv_event_get_user_data(event);auto s=victron_snapshot();
    if(victron_relay_available(s,relay))victron_request_relay(relay,s.values[GX_RELAY0+relay].value==0);
    ui_update_victron();
}
void open_detail(lv_event_t*) {
    if(ui_page_available(TAB_VICTRON)){ui_navigate_to(TAB_VICTRON);return;}
    state.tab_enabled[TAB_VICTRON]=true;state_save();ui_init();
    lv_timer_t *timer=lv_timer_create([](lv_timer_t *timer){ui_navigate_to(TAB_VICTRON);},100,nullptr);
    if(timer)lv_timer_set_repeat_count(timer,1);
}
}
void ui_victron_reset(){off_feedback_active=false;root=heading=status=ac_in=ac_out=dc=extra=nullptr;detail_modes=ModeWidgets();for(int i=0;i<2;++i)relay_names[i]=relay_buttons[i]=relay_labels[i]=nullptr;}
void ui_victron_home_reset(){off_feedback_active=false;home_root=home_title=home_power_root=nullptr;home_modes=ModeWidgets();for(int i=0;i<3;++i)home_power_labels[i]=home_power_values[i]=nullptr;}
lv_obj_t *ui_build_victron_home(lv_obj_t *parent) {
    home_root=ui_create_card(parent,464,80);lv_obj_set_style_pad_all(home_root,0,0);
    home_title=label(home_root,"MultiPlus-II",6,2,&lv_font_montserrat_12);
    lv_obj_set_width(home_title,450);lv_label_set_long_mode(home_title,LV_LABEL_LONG_DOT);
    lv_obj_add_flag(home_title,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(home_title,open_detail,LV_EVENT_CLICKED,nullptr);
    build_modes(home_root,home_modes,0,22,464);
    ui_update_victron_home();return home_root;
}
lv_obj_t *ui_build_victron_home_power(lv_obj_t *parent) {
    home_power_root=ui_create_card(parent,104,122);lv_obj_set_style_pad_all(home_power_root,6,0);
    lv_obj_add_flag(home_power_root,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(home_power_root,open_detail,LV_EVENT_CLICKED,nullptr);
    const char *captions[]={"MP AC IN","AC OUT","DC"};
    for(int i=0;i<3;++i) {
        home_power_labels[i]=label(home_power_root,captions[i],0,i*34,&lv_font_montserrat_12);
        home_power_values[i]=label(home_power_root,"--",0,i*34+14,&lv_font_montserrat_20);
        lv_obj_set_width(home_power_labels[i],LV_PCT(100));lv_obj_set_width(home_power_values[i],LV_PCT(100));
        lv_label_set_long_mode(home_power_labels[i],LV_LABEL_LONG_DOT);
        lv_label_set_long_mode(home_power_values[i],LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(home_power_labels[i],ui_theme_muted(),0);
        lv_obj_set_style_text_color(home_power_values[i],lv_color_hex(UI_COLOR_WARNING),0);
    }
    ui_update_victron_home();return home_power_root;
}
void ui_update_victron_home() {
    if(!home_root&&!home_power_root)return;auto s=victron_snapshot();char line[160];
    if(home_root) {
        const char *state_text=victron_fresh(s,GX_STATE)?victron_state_name((int)s.values[GX_STATE].value):victron_status(s);
        if(s.pending[0].active()||s.pending[0].phase==GxCommandPhase::Timeout||s.pending[0].phase==GxCommandPhase::Failed)state_text=victron_status(s);
        snprintf(line,sizeof(line),"MultiPlus-II  %s %s  " LV_SYMBOL_RIGHT,s.demo?"Demo |":"",state_text);
        ui_label_set_text_if_changed(home_title,line);update_modes(home_modes,s);
    }
    if(home_power_root) {
        formatted(line,sizeof(line),s,input_power(s),"%.0fW");ui_label_set_text_if_changed(home_power_values[0],line);
        formatted(line,sizeof(line),s,output_power(s),"%.0fW");ui_label_set_text_if_changed(home_power_values[1],line);
        if(victron_fresh(s,GX_DC_V)&&victron_fresh(s,GX_DC_I))
            snprintf(line,sizeof(line),"%+.0fW",s.values[GX_DC_V].value*s.values[GX_DC_I].value);
        else strcpy(line,"--");
        ui_label_set_text_if_changed(home_power_values[2],line);
    }
}
void ui_build_victron(lv_obj_t *parent) {
    root=parent;ui_setup_tab_page(parent);lv_obj_add_flag(parent,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(parent,LV_DIR_VER);lv_obj_set_style_pad_bottom(parent,12,0);
    heading=label(parent,"Victron / MultiPlus-II",0,0,&lv_font_montserrat_16);lv_obj_set_width(heading,384);
    lv_obj_t *settings_button=lv_btn_create(parent);
    lv_obj_set_pos(settings_button,396,0);lv_obj_set_size(settings_button,44,44);
    lv_obj_set_style_radius(settings_button,12,0);
    lv_obj_set_style_bg_color(settings_button,ui_theme_card(),0);
    lv_obj_set_style_border_width(settings_button,1,0);
    lv_obj_set_style_border_color(settings_button,ui_theme_border(),0);
    lv_obj_add_event_cb(settings_button,[](lv_event_t *){ui_open_victron_settings();},LV_EVENT_CLICKED,nullptr);
    lv_obj_t *gear=label(settings_button,MDI_COG,0,0,&ui_font_mdi_24);
    lv_obj_set_style_text_color(gear,lv_color_hex(UI_COLOR_PRIMARY),0);lv_obj_center(gear);
    lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);lv_obj_set_style_text_color(heading,lv_color_hex(UI_COLOR_PRIMARY),0);
    status=label(parent,"--",0,26,&lv_font_montserrat_12);lv_obj_set_width(status,384);
    lv_label_set_long_mode(status,LV_LABEL_LONG_DOT);
    build_modes(parent,detail_modes,0,52,440);
    lv_obj_t *input=ui_create_card(parent,216,90);lv_obj_set_pos(input,0,118);
    label(input,"AC IN (L1)",0,0,&lv_font_montserrat_14);
    ac_in=label(input,"--",0,23,&lv_font_montserrat_14);lv_obj_set_width(ac_in,190);
    lv_obj_t *output=ui_create_card(parent,216,90);lv_obj_set_pos(output,224,118);
    label(output,"AC OUT (L1)",0,0,&lv_font_montserrat_14);
    ac_out=label(output,"--",0,23,&lv_font_montserrat_14);lv_obj_set_width(ac_out,190);
    dc=label(parent,"--",0,222,&lv_font_montserrat_14);lv_obj_set_width(dc,440);
    for(int i=0;i<2;++i) {
        lv_obj_t *card=ui_create_card(parent,440,64);lv_obj_set_pos(card,0,266+i*72);
        relay_names[i]=label(card,"--",0,0,&lv_font_montserrat_14);lv_obj_set_width(relay_names[i],265);
        lv_label_set_long_mode(relay_names[i],LV_LABEL_LONG_DOT);
        relay_labels[i]=label(card,"--",0,23,&lv_font_montserrat_12);lv_obj_set_width(relay_labels[i],265);
        lv_label_set_long_mode(relay_labels[i],LV_LABEL_LONG_DOT);
        relay_buttons[i]=lv_btn_create(card);lv_obj_set_size(relay_buttons[i],112,44);lv_obj_set_style_radius(relay_buttons[i],12,0);
        lv_obj_align(relay_buttons[i],LV_ALIGN_RIGHT_MID,0,0);
        lv_obj_set_style_bg_color(relay_buttons[i],ui_theme_track(),0);
        lv_obj_set_style_bg_color(relay_buttons[i],lv_color_hex(UI_COLOR_PRIMARY),LV_STATE_CHECKED);
        lv_obj_add_event_cb(relay_buttons[i],relay_click,LV_EVENT_CLICKED,(void*)(intptr_t)i);
        lv_obj_t *button_label=label(relay_buttons[i],"--",0,0,&lv_font_montserrat_16);lv_obj_center(button_label);
    }
    extra=label(parent,"--",0,418,&lv_font_montserrat_12);lv_obj_set_width(extra,440);
    ui_update_victron();
}
void ui_update_victron() {
    if(!root)return;auto s=victron_snapshot();char line[220],v[20],i[20],p[20],f[20],soc[20],pv[20];
    ui_label_set_text_if_changed(heading,s.product[0]?s.product:"Victron / MultiPlus-II");
    const char *mode=victron_fresh(s,GX_MODE)?victron_mode_name((int)s.values[GX_MODE].value):"--";
    const char *state_text=victron_fresh(s,GX_STATE)?victron_state_name((int)s.values[GX_STATE].value):"--";
    snprintf(line,sizeof(line),"%s | %s | %s",mode,state_text,victron_status(s));ui_label_set_text_if_changed(status,line);
    update_modes(detail_modes,s);
    for(int side=0;side<2;++side) {
        formatted(v,sizeof(v),s,side?GX_OUT_V:GX_IN_V,"%.1f");formatted(i,sizeof(i),s,side?GX_OUT_I:GX_IN_I,"%.1f");
        formatted(p,sizeof(p),s,side?GX_OUT_P:GX_IN_P,"%.0f");
        snprintf(line,sizeof(line),"%s W\n%s V   %s A",p,v,i);ui_label_set_text_if_changed(side?ac_out:ac_in,line);
    }
    formatted(f,sizeof(f),s,victron_fresh(s,GX_IN_F)?GX_IN_F:GX_OUT_F,"%.1f");
    bool dc_valid=victron_fresh(s,GX_DC_V)&&victron_fresh(s,GX_DC_I);
    if(dc_valid)snprintf(p,sizeof(p),"%+.0f",s.values[GX_DC_V].value*s.values[GX_DC_I].value);else strcpy(p,"--");
    snprintf(line,sizeof(line),"Frequenz: %s Hz\nDC-Leistung: %s W",f,p);ui_label_set_text_if_changed(dc,line);
    for(int relay=0;relay<2;++relay) {
        ui_label_set_text_if_changed(relay_names[relay],state.victron_relay_names[relay].c_str());
        bool ready=victron_relay_available(s,relay),valid=victron_fresh(s,(GxField)(GX_RELAY0+relay));
        bool on=valid&&s.values[GX_RELAY0+relay].value==1;
        lv_obj_t *button_label=lv_obj_get_child(relay_buttons[relay],0);
        ui_label_set_text_if_changed(button_label,s.pending[relay+1].active()?"Warte":valid?on?"EIN":"AUS":"--");
        set_button(relay_buttons[relay],button_label,on,ready);
        const char *info=ready?"Manuelle Steuerung":s.pending[relay+1].active()?"Warte auf Rueckmeldung":
            s.pending[relay+1].phase==GxCommandPhase::Timeout?"Keine Bestaetigung":
            s.pending[relay+1].phase==GxCommandPhase::Failed?"Befehl fehlgeschlagen: Zustand pruefen":
            !s.enabled||s.stale?"Keine aktuellen GX-Daten":"GX-Funktion: Manuell pruefen";
        ui_label_set_text_if_changed(relay_labels[relay],info);
    }
    formatted(soc,sizeof(soc),s,GX_SOC,"%.0f");formatted(pv,sizeof(pv),s,GX_PV,"%.0f");
    snprintf(line,sizeof(line),"GX Batterie: %s %%   Solar: %s W\nPortal: %s | VE.Bus: %d",soc,pv,s.portal[0]?s.portal:"Auto / warte",s.instance);
    ui_label_set_text_if_changed(extra,line);
}
