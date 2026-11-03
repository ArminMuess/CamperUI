#include "ui_energy_history.h"
#include "ui_main.h"
#include "ui_chart_style.h"
#include "energy_history.h"
#include <stdio.h>

namespace {
lv_obj_t *screen, *chart, *summary, *notice, *axis_start, *axis_mid, *scale_min, *scale_max, *metric_buttons[2], *window_buttons[4];
lv_chart_series_t *series;
int track, window=1;
uint32_t rendered=UINT32_MAX;
const int windows[4]={1,6,12,24};
constexpr int points_count=120;
// For 1 h render 60 points; longer windows use 120 aggregate buckets.
lv_obj_t *text(lv_obj_t *parent,const char *value,int x,int y,const lv_font_t *font) {
    lv_obj_t *label=lv_label_create(parent); lv_label_set_text(label,value);
    lv_obj_set_pos(label,x,y); lv_obj_set_style_text_font(label,font,0);
    lv_obj_set_style_text_color(label,ui_theme_text(),0); return label;
}
void style_button(lv_obj_t *b,bool active) {
    ui_chart_button_style(b,active);
}
lv_obj_t *button(lv_obj_t *parent,const char *caption,int x,int y,int width,lv_event_cb_t callback,int data) {
    lv_obj_t *b=lv_btn_create(parent); lv_obj_set_pos(b,x,y); lv_obj_set_size(b,width,44);
    lv_obj_set_style_radius(b,8,0);
    lv_obj_add_event_cb(b,callback,LV_EVENT_CLICKED,(void*)(intptr_t)data);
    lv_obj_t *label=lv_label_create(b); lv_label_set_text(label,caption);
    lv_obj_set_style_text_font(label,&lv_font_montserrat_14,0); lv_obj_center(label);
    style_button(b,false); return b;
}
void close_screen(void*) {
    if(!screen)return;
    if(scr_main)lv_scr_load(scr_main);
    ui_energy_history_destroy();
    if(scr_main)ui_update_visible_page();
}
void close_clicked(lv_event_t*) { lv_async_call(close_screen,nullptr); }
void selection_clicked(lv_event_t *e) {
    int value=(int)(intptr_t)lv_event_get_user_data(e);
    if(value<0) track=-value-1; else window=value;
    rendered=UINT32_MAX; ui_energy_history_update();
}
void open_screen(void *arg) {
    if(screen || !scr_main)return;
    lv_mem_monitor_t memory; lv_mem_monitor(&memory);
    if (memory.free_biggest_size < 32768) {
        lv_obj_t *box=lv_msgbox_create(nullptr,"Verlauf","Zu wenig freier UI-Speicher.",nullptr,true);
        if(box)lv_obj_center(box);
        return;
    }
    track=(int)(intptr_t)arg; window=1; rendered=UINT32_MAX;
    screen=lv_obj_create(nullptr); lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen,480,480); lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(screen,ui_theme_bg(),0);
    button(screen,LV_SYMBOL_LEFT " Zurück",12,10,108,close_clicked,0);
    lv_obj_t *title=text(screen,track==2?"Solar-Verlauf":"Batterie-Verlauf",136,22,&lv_font_montserrat_16);
    lv_obj_set_style_text_color(title,lv_color_hex(UI_COLOR_PRIMARY),0);
    metric_buttons[0]=metric_buttons[1]=nullptr;
    if(track!=2) {
        metric_buttons[0]=button(screen,"Ladezustand (%)",12,64,222,selection_clicked,-1);
        metric_buttons[1]=button(screen,"Leistung (W)",246,64,222,selection_clicked,-2);
    } else {
        text(screen,"Solarleistung (W)",18,76,&lv_font_montserrat_18);
    }
    for(int i=0;i<4;++i) {
        char caption[12]; snprintf(caption,sizeof(caption),"%dh",windows[i]);
        window_buttons[i]=button(screen,caption,12+i*116,118,108,selection_clicked,windows[i]);
    }
    summary=text(screen,"--",18,174,&lv_font_montserrat_14);
    lv_obj_t *card=ui_create_card(screen,440,222); lv_obj_set_pos(card,20,204);
    ui_chart_card_style(card);
    scale_max=text(card,"Max: --",6,2,&lv_font_montserrat_12);
    scale_min=text(card,"Min: --",0,0,&lv_font_montserrat_12);
    lv_obj_align(scale_min,LV_ALIGN_TOP_RIGHT,-6,2);
    lv_obj_set_style_text_color(scale_max,ui_theme_muted(),0);
    lv_obj_set_style_text_color(scale_min,ui_theme_muted(),0);
    chart=lv_chart_create(card); lv_obj_set_pos(chart,52,26); lv_obj_set_size(chart,370,162);
    lv_chart_set_type(chart,LV_CHART_TYPE_LINE);
    lv_chart_set_axis_tick(chart,LV_CHART_AXIS_PRIMARY_Y,5,0,5,1,true,52);
    ui_chart_grid_style(chart);
    lv_obj_set_style_pad_all(chart,0,0);
    lv_chart_set_div_line_count(chart,4,0);
    lv_obj_clear_flag(chart,LV_OBJ_FLAG_SCROLLABLE);
    series=lv_chart_add_series(chart,lv_color_hex(UI_COLOR_PRIMARY),LV_CHART_AXIS_PRIMARY_Y);
    axis_start=text(card,"-1 Std.",0,0,&lv_font_montserrat_12);
    axis_mid=text(card,"-30 Min.",0,0,&lv_font_montserrat_12);
    lv_obj_t *axis_now=text(card,"Jetzt",0,0,&lv_font_montserrat_12);
    lv_obj_align(axis_start,LV_ALIGN_BOTTOM_LEFT,6,-2);
    lv_obj_align(axis_mid,LV_ALIGN_BOTTOM_MID,0,-2);
    lv_obj_align(axis_now,LV_ALIGN_BOTTOM_RIGHT,-6,-2);
    for(lv_obj_t *axis : {axis_start,axis_mid,axis_now})
        lv_obj_set_style_text_color(axis,ui_theme_muted(),0);
    notice=text(screen,"",18,436,&lv_font_montserrat_12);
    lv_obj_set_width(notice,444); lv_label_set_long_mode(notice,LV_LABEL_LONG_WRAP);
    lv_scr_load(screen); ui_energy_history_update();
}
void chart_clicked(lv_event_t *e) {
    if(lv_async_call(open_screen,lv_event_get_user_data(e))!=LV_RES_OK) {
        lv_obj_t *box=lv_msgbox_create(nullptr,"Verlauf","Nicht genug Speicher.",nullptr,true);
        if(box)lv_obj_center(box);
    }
}
}
bool ui_energy_history_active() { return screen && lv_scr_act()==screen; }
void ui_energy_history_destroy() {
    if(!screen)return;
    lv_obj_t *old=screen; screen=chart=summary=notice=axis_start=axis_mid=scale_min=scale_max=nullptr; series=nullptr;
    lv_obj_del(old);
}
void ui_energy_history_button(lv_obj_t *card,bool solar) {
    lv_obj_t *b=button(card,"",0,0,48,chart_clicked,solar?2:0);
    lv_obj_align(b,LV_ALIGN_TOP_RIGHT,0,0);
    lv_obj_set_style_pad_all(b,8,0);
    static const lv_point_t axes[]={{0,0},{0,22},{26,22}};
    static const lv_point_t curve[]={{2,17},{9,11},{16,15},{25,3}};
    for(int i=0;i<2;++i) {
        lv_obj_t *line=lv_line_create(b); lv_line_set_points(line,i?curve:axes,i?4:3);
        lv_obj_set_pos(line,0,0); lv_obj_set_style_line_width(line,2,0);
        lv_obj_set_style_line_color(line,lv_color_hex(UI_COLOR_PRIMARY),0);
        lv_obj_clear_flag(line,LV_OBJ_FLAG_CLICKABLE);
    }
}
void ui_energy_history_update() {
    if(!ui_energy_history_active())return;
    uint32_t version=energy_history_version();
    if(rendered==version)return;
    rendered=version;
    for(int i=0;i<4;++i)style_button(window_buttons[i],window==windows[i]);
    for(int i=0;i<2;++i)if(metric_buttons[i])style_button(metric_buttons[i],track==i);
    char start[20]; snprintf(start,sizeof(start),"-%d Std.",window); lv_label_set_text(axis_start,start);
    if(window==1)lv_label_set_text(axis_mid,"-30 Min.");
    else { snprintf(start,sizeof(start),"-%d Std.",window/2); lv_label_set_text(axis_mid,start); }
    // Five grid lines preserve a visible zero line for signed battery watts.
    lv_chart_set_div_line_count(chart,track==1?5:4,0);
    lv_chart_set_axis_tick(chart,LV_CHART_AXIS_PRIMARY_Y,5,0,track==1?5:4,1,true,50);
    int count=window==1?60:points_count; lv_chart_set_point_count(chart,count);
    lv_coord_t values[points_count]; int low,high;
    bool has_data=energy_history_series(track,window,values,count,&low,&high);
    int lower=0,upper=100;
    if(track!=0) {
        int span=high-low; if(span<20)span=20;
        lower=low-span/10-5; upper=high+span/10+5;
        if(track==2)lower=0;
        else {
            int extent = high > -low ? high : -low;
            if (extent < 10) extent = 10;
            extent += extent/10+5;
            lower=-extent; upper=extent;
        }
        if(upper<=lower)upper=lower+20;
        // LVGL uses 16-bit chart coordinates.
        if(lower<-32767)lower=-32767; if(upper>32766)upper=32766;
    }
    lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,lower,upper);
    lv_chart_set_series_color(chart,series,lv_color_hex(track==2?UI_COLOR_WARNING:UI_COLOR_PRIMARY));
    for(int i=0;i<count;++i)series->y_points[i]=values[i];
    lv_chart_refresh(chart);
    char caption[96];
    if(has_data) {
        snprintf(caption,sizeof(caption),"Max: %d %s",high,track==0?"%":"W"); lv_label_set_text(scale_max,caption);
        snprintf(caption,sizeof(caption),"Min: %d %s",low,track==0?"%":"W"); lv_label_set_text(scale_min,caption);
        snprintf(caption,sizeof(caption),"%s",track==0?"Ladezustand":track==1?"Lade- / Entladeleistung":"Solarleistung");
    } else {
        lv_label_set_text(scale_max,"Max: --"); lv_label_set_text(scale_min,"Min: --");
        snprintf(caption,sizeof(caption),"%s",energy_history_available()?"Noch keine vollständige Messminute":"Verlauf-Speicher nicht verfügbar");
    }
    lv_obj_align(scale_min,LV_ALIGN_TOP_RIGHT,-6,2);
    lv_label_set_text(summary,caption);
    const char *source=energy_history_demo()?"Demo":"Live";
    snprintf(caption,sizeof(caption),"%s | Minutenmittel, ab Start%s",source,
        track==1?"\n+ Laden / - Entladen":"");
    lv_label_set_text(notice,caption);
}
