#pragma once
#include "ui_main.h"
constexpr int UI_FAVORITE_HEIGHT=56;
inline void ui_favorite_button_style(lv_obj_t *button) {
    lv_obj_set_height(button,UI_FAVORITE_HEIGHT);lv_obj_set_style_radius(button,ui_radius_button(),0);
    lv_obj_set_style_bg_color(button,ui_theme_card(),0);
    lv_obj_set_style_border_width(button,ui_border_width(),0);lv_obj_set_style_border_color(button,ui_theme_border(),0);
    lv_obj_set_style_bg_color(button,ui_theme_primary(),LV_STATE_CHECKED);
    lv_obj_set_style_text_color(button,lv_color_hex(0xffffff),LV_STATE_CHECKED);
}
