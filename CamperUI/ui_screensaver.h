#ifndef UI_SCREENSAVER_H
#define UI_SCREENSAVER_H

#include <lvgl.h>

void ui_screensaver_init();
void ui_screensaver_enter();
void ui_screensaver_exit();
bool ui_screensaver_is_active();
void ui_screensaver_update();
uint8_t ui_screensaver_brightness_to_pwm(int brightness_pct);

#endif // UI_SCREENSAVER_H

