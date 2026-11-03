#ifndef UI_FONTS_H
#define UI_FONTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fallback fonts containing German umlauts (Ä, Ö, Ü, ß, ä, ö, ü, €)
LV_FONT_DECLARE(ui_font_de_10);
LV_FONT_DECLARE(ui_font_de_12);
LV_FONT_DECLARE(ui_font_de_14);
LV_FONT_DECLARE(ui_font_de_16);
LV_FONT_DECLARE(ui_font_de_18);
LV_FONT_DECLARE(ui_font_de_20);
LV_FONT_DECLARE(ui_font_de_22);
LV_FONT_DECLARE(ui_font_de_24);
LV_FONT_DECLARE(ui_font_de_26);
LV_FONT_DECLARE(ui_font_de_28);
LV_FONT_DECLARE(ui_font_de_30);
LV_FONT_DECLARE(ui_font_de_32);
LV_FONT_DECLARE(ui_font_de_42);
LV_FONT_DECLARE(ui_font_clock_120);

// Augmented Montserrat fonts (with attached German umlaut fallbacks)
extern lv_font_t ui_font_montserrat_10;
extern lv_font_t ui_font_montserrat_12;
extern lv_font_t ui_font_montserrat_14;
extern lv_font_t ui_font_montserrat_16;
extern lv_font_t ui_font_montserrat_18;
extern lv_font_t ui_font_montserrat_20;
extern lv_font_t ui_font_montserrat_22;
extern lv_font_t ui_font_montserrat_24;
extern lv_font_t ui_font_montserrat_26;
extern lv_font_t ui_font_montserrat_28;
extern lv_font_t ui_font_montserrat_30;
extern lv_font_t ui_font_montserrat_32;
extern lv_font_t ui_font_montserrat_42;

// Initialize font wrappers and attach fallback fonts
void ui_fonts_init(void);

#ifdef __cplusplus
}
#endif

// Redirect standard Montserrat font references in UI code to augmented versions
#define lv_font_montserrat_10 ui_font_montserrat_10
#define lv_font_montserrat_12 ui_font_montserrat_12
#define lv_font_montserrat_14 ui_font_montserrat_14
#define lv_font_montserrat_16 ui_font_montserrat_16
#define lv_font_montserrat_18 ui_font_montserrat_18
#define lv_font_montserrat_20 ui_font_montserrat_20
#define lv_font_montserrat_22 ui_font_montserrat_22
#define lv_font_montserrat_24 ui_font_montserrat_24
#define lv_font_montserrat_26 ui_font_montserrat_26
#define lv_font_montserrat_28 ui_font_montserrat_28
#define lv_font_montserrat_30 ui_font_montserrat_30
#define lv_font_montserrat_32 ui_font_montserrat_32
#define lv_font_montserrat_42 ui_font_montserrat_42

#endif // UI_FONTS_H

