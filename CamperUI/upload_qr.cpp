#include "upload_qr.h"
#include "camper_qrcodegen.h"
#include <string.h>

static constexpr int SIZE = 180;
lv_obj_t *upload_qr_create(lv_obj_t *parent) {
    void *buffer = lv_mem_alloc(LV_CANVAS_BUF_SIZE_INDEXED_1BIT(SIZE, SIZE));
    if (!buffer) return nullptr;
    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, buffer, SIZE, SIZE, LV_IMG_CF_INDEXED_1BIT);
    lv_canvas_set_palette(canvas, 0, lv_color_hex(0x000000));
    lv_canvas_set_palette(canvas, 1, lv_color_hex(0xffffff));
    lv_obj_add_event_cb(canvas, [](lv_event_t *e) { lv_mem_free(lv_event_get_user_data(e)); }, LV_EVENT_DELETE, buffer);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return canvas;
}
bool upload_qr_update(lv_obj_t *canvas, const char *url) {
    if (!canvas || !url || strlen(url) > 64) return false;
    uint8_t temp[camper_qrcodegen_BUFFER_LEN_FOR_VERSION(4)], qr[camper_qrcodegen_BUFFER_LEN_FOR_VERSION(4)];
    if (!camper_qrcodegen_encodeText(url, temp, qr, camper_qrcodegen_Ecc_MEDIUM, 1, 4, camper_qrcodegen_Mask_AUTO, true)) return false;
    int modules = camper_qrcodegen_getSize(qr);
    // Four modules of white quiet zone on every side, including in light mode.
    int scale = SIZE / (modules + 8), margin = (SIZE - modules * scale) / 2;
    lv_color_t white; white.full = 1;
    lv_canvas_fill_bg(canvas, white, LV_OPA_COVER);
    lv_color_t black; black.full = 0;
    for (int y = 0; y < modules; ++y) for (int x = 0; x < modules; ++x) {
        if (!camper_qrcodegen_getModule(qr, x, y)) continue;
        for (int dy = 0; dy < scale; ++dy) for (int dx = 0; dx < scale; ++dx)
            lv_canvas_set_px_color(canvas, margin + x * scale + dx, margin + y * scale + dy, black);
    }
    return true;
}
