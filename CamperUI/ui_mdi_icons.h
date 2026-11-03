#ifndef UI_MDI_ICONS_H
#define UI_MDI_ICONS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Material Design Icons Unicode PUA Mappings (0xE001 .. 0xE018)
#define MDI_LIGHTBULB          "\xEE\x80\x81" // 0xE001: Dimmer / Licht
#define MDI_BATTERY_CHARGING   "\xEE\x80\x82" // 0xE002: Power
#define MDI_WATER              "\xEE\x80\x83" // 0xE003: Wasser
#define MDI_THERMOMETER        "\xEE\x80\x84" // 0xE004: Klima
#define MDI_TOGGLE_SWITCH      "\xEE\x80\x85" // 0xE005: Relais
#define MDI_SPIRIT_LEVEL       "\xEE\x80\x86" // 0xE006: Wasserwaage
#define MDI_TUNE               "\xEE\x80\x87" // 0xE007: Setup / Einstellungen
#define MDI_WIFI               "\xEE\x80\x88" // 0xE008: Header WiFi
#define MDI_BATTERY            "\xEE\x80\x89" // 0xE009: Header Battery
#define MDI_PUMP               "\xEE\x80\x8A" // 0xE00A: Header Pump
#define MDI_FIRE               "\xEE\x80\x8B" // 0xE00B: Header Heater / Fire
#define MDI_SNOWFLAKE          "\xEE\x80\x8C" // 0xE00C: Header Frost
#define MDI_ALERT              "\xEE\x80\x8D" // 0xE00D: Alert
#define MDI_SOLAR              "\xEE\x80\x8E" // 0xE00E: Solar Power
#define MDI_CHECK              "\xEE\x80\x8F" // 0xE00F: Check OK

#define MDI_HOME               "\xEE\x80\x90" // 0xE010: Home

#define MDI_FAN                "\xEE\x80\x91" // 0xE011: MaxxFan

#define MDI_COG                "\xEE\x80\x92" // 0xE012: Settings / Allgemein
#define MDI_EYE                "\xEE\x80\x93" // 0xE013: Anzeige / Display
#define MDI_SLIDERS            "\xEE\x80\x94" // 0xE014: Werte / Parameter
#define MDI_BELL               "\xEE\x80\x95" // 0xE015: Alarme
#define MDI_CLOCK              "\xEE\x80\x96" // 0xE016: Zeit / Uhr
#define MDI_LOCK               "\xEE\x80\x97" // 0xE017: Lock / Sperre
#define MDI_STETHOSCOPE               "\xEE\x80\x98" // 0xE018: Stethoskop / Diagnose

LV_FONT_DECLARE(ui_font_mdi_32);
LV_FONT_DECLARE(ui_font_mdi_24);
LV_FONT_DECLARE(ui_font_mdi_18);

#ifdef __cplusplus
}
#endif

#endif // UI_MDI_ICONS_H
