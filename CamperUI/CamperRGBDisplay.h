// Project-local adaptation of Arduino_GFX 1.5.3 (Moon On Our Nation).
// See Arduino_GFX-license.txt for the retained BSD license.
#pragma once
#include <Arduino_GFX_Library.h>
#include "CamperRGBPanel.h"
#if defined(ESP32) && CONFIG_IDF_TARGET_ESP32S3
class CamperRGBDisplay : public Arduino_GFX
{
public:
    CamperRGBDisplay(
        int16_t w, int16_t h, CamperRGBPanel *rgbpanel, uint8_t r = 0, bool auto_flush = true,
        Arduino_DataBus *bus = NULL, int8_t rst = GFX_NOT_DEFINED, const uint8_t *init_operations = NULL, size_t init_operations_len = GFX_NOT_DEFINED,
        uint8_t col_offset1 = 0, uint8_t row_offset1 = 0, uint8_t col_offset2 = 0, uint8_t row_offset2 = 0);

    bool begin(int32_t speed = GFX_NOT_DEFINED) override;
    void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override;
    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
    void writeFastVLineCore(int16_t x, int16_t y, int16_t h, uint16_t color);
    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
    void writeFastHLineCore(int16_t x, int16_t y, int16_t w, uint16_t color);
    void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void drawIndexedBitmap(int16_t x, int16_t y, uint8_t *bitmap, uint16_t *color_index, int16_t w, int16_t h, int16_t x_skip = 0) override;
    void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) override;
    void draw16bitBeRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) override;
    void flush(bool force_flush = false) override;

    uint16_t *getFramebuffer();

protected:
    uint16_t *_framebuffer;
    size_t _framebuffer_size;
    CamperRGBPanel *_rgbpanel;
    bool _auto_flush;
    Arduino_DataBus *_bus;
    int8_t _rst;
    const uint8_t *_init_operations;
    size_t _init_operations_len;
    int16_t MAX_X, MAX_Y;
    uint8_t COL_OFFSET1, ROW_OFFSET1;
    uint8_t COL_OFFSET2, ROW_OFFSET2;
    uint8_t _xStart, _yStart;
    uint16_t _fb_width, _fb_height, _fb_max_x, _fb_max_y;

private:
};

#endif // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)
