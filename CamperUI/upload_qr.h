#pragma once
#include <lvgl.h>
lv_obj_t *upload_qr_create(lv_obj_t *parent);
bool upload_qr_update(lv_obj_t *canvas, const char *url);
