#pragma once
#include "ui_main.h"

// Common visual language for temperature, battery and solar histories.
inline void ui_chart_button_style(lv_obj_t *button, bool active) {
    lv_obj_set_style_radius(button, ui_radius_button(), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_border_width(button, ui_border_width(), 0);
    lv_obj_set_style_border_color(button, ui_theme_border(), 0);
    lv_obj_set_style_bg_color(button, active ? ui_theme_primary() : ui_theme_track(), 0);
    lv_obj_set_style_text_color(button, active ? lv_color_hex(0xffffff) : ui_theme_muted(), 0);
}
inline void ui_chart_card_style(lv_obj_t *card) {
    lv_obj_set_style_bg_color(card, ui_theme_card(), 0);
    lv_obj_set_style_border_color(card, ui_theme_border(), 0);
    lv_obj_set_style_border_width(card, ui_border_width(), 0);
    lv_obj_set_style_radius(card, ui_radius_card(), 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
}
inline void ui_chart_grid_style(lv_obj_t *chart) {
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_line_color(chart, ui_theme_border(), LV_PART_MAIN);
    lv_obj_set_style_line_opa(chart, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS);
    lv_obj_set_style_text_font(chart, &lv_font_montserrat_12, LV_PART_TICKS);
    lv_obj_set_style_text_color(chart, ui_theme_muted(), LV_PART_TICKS);
}
