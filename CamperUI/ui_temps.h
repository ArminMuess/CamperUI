#ifndef UI_TEMPS_H
#define UI_TEMPS_H

#include <lvgl.h>

void ui_build_temps(lv_obj_t *parent);
void ui_update_temps_tab(void);
void ui_trigger_temps_anim(void);
void ui_temps_reset(void);

#endif // UI_TEMPS_H
