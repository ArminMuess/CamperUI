#include "ui_fonts.h"
#include <Arduino.h>

// Undefine macros so this file can access the real library fonts
#undef lv_font_montserrat_10
#undef lv_font_montserrat_12
#undef lv_font_montserrat_14
#undef lv_font_montserrat_16
#undef lv_font_montserrat_18
#undef lv_font_montserrat_20
#undef lv_font_montserrat_22
#undef lv_font_montserrat_24
#undef lv_font_montserrat_26
#undef lv_font_montserrat_28
#undef lv_font_montserrat_30
#undef lv_font_montserrat_32
#undef lv_font_montserrat_42

extern "C" {
lv_font_t ui_font_montserrat_10;
lv_font_t ui_font_montserrat_12;
lv_font_t ui_font_montserrat_14;
lv_font_t ui_font_montserrat_16;
lv_font_t ui_font_montserrat_18;
lv_font_t ui_font_montserrat_20;
lv_font_t ui_font_montserrat_22;
lv_font_t ui_font_montserrat_24;
lv_font_t ui_font_montserrat_26;
lv_font_t ui_font_montserrat_28;
lv_font_t ui_font_montserrat_30;
lv_font_t ui_font_montserrat_32;
lv_font_t ui_font_montserrat_42;
}

#include "ui_mdi_icons.h"

static lv_font_t ui_font_de_16_wrapper;
static lv_font_t ui_font_de_18_wrapper;
static lv_font_t ui_font_de_20_wrapper;
static lv_font_t ui_font_de_22_wrapper;
static lv_font_t ui_font_de_24_wrapper;
static lv_font_t ui_font_de_32_wrapper;

void ui_fonts_init(void) {
    ui_font_montserrat_10 = lv_font_montserrat_10;
    ui_font_montserrat_10.fallback = &ui_font_de_10;

    ui_font_montserrat_12 = lv_font_montserrat_12;
    ui_font_montserrat_12.fallback = &ui_font_de_12;

    ui_font_montserrat_14 = lv_font_montserrat_14;
    ui_font_montserrat_14.fallback = &ui_font_de_14;

    ui_font_de_16_wrapper = ui_font_de_16;
    ui_font_de_16_wrapper.fallback = &ui_font_mdi_18;
    ui_font_montserrat_16 = lv_font_montserrat_16;
    ui_font_montserrat_16.fallback = &ui_font_de_16_wrapper;

    ui_font_de_18_wrapper = ui_font_de_18;
    ui_font_de_18_wrapper.fallback = &ui_font_mdi_18;
    ui_font_montserrat_18 = lv_font_montserrat_18;
    ui_font_montserrat_18.fallback = &ui_font_de_18_wrapper;

    ui_font_de_20_wrapper = ui_font_de_20;
    ui_font_de_20_wrapper.fallback = &ui_font_mdi_18;
    ui_font_montserrat_20 = lv_font_montserrat_20;
    ui_font_montserrat_20.fallback = &ui_font_de_20_wrapper;

    ui_font_de_22_wrapper = ui_font_de_22;
    ui_font_de_22_wrapper.fallback = &ui_font_mdi_18;
    ui_font_montserrat_22 = lv_font_montserrat_22;
    ui_font_montserrat_22.fallback = &ui_font_de_22_wrapper;

    ui_font_de_24_wrapper = ui_font_de_24;
    ui_font_de_24_wrapper.fallback = &ui_font_mdi_24;
    ui_font_montserrat_24 = lv_font_montserrat_24;
    ui_font_montserrat_24.fallback = &ui_font_de_24_wrapper;

    ui_font_montserrat_26 = lv_font_montserrat_26;
    ui_font_montserrat_26.fallback = &ui_font_de_26;

    ui_font_montserrat_28 = lv_font_montserrat_28;
    ui_font_montserrat_28.fallback = &ui_font_de_28;

    ui_font_montserrat_30 = lv_font_montserrat_30;
    ui_font_montserrat_30.fallback = &ui_font_de_30;

    ui_font_de_32_wrapper = ui_font_de_32;
    ui_font_de_32_wrapper.fallback = &ui_font_mdi_32;
    ui_font_montserrat_32 = lv_font_montserrat_32;
    ui_font_montserrat_32.fallback = &ui_font_de_32_wrapper;

    ui_font_montserrat_42 = lv_font_montserrat_42;
    ui_font_montserrat_42.fallback = &ui_font_de_42;
}

