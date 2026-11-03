// Project-local adaptation of Arduino_GFX 1.5.3 (Moon On Our Nation).
// See Arduino_GFX-license.txt for the retained BSD license.
#pragma once

// ESP_LCD_Panel implementation for esp32s3.

// This panel implementation requires a hardware setup with
//  * RGB LCD peripheral supported (esps3 for now)
//  * Octal PSRAM onboard
//  * RGB panel, 16 bit-width, with HSYNC, VSYNC and DE signal
//
// It uses a Single Frame Buffer in PSRAM
//
// See: (ESP32 board version 3.x)
// * https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html
// * https://github.com/espressif/esp-idf/blob/master/examples/peripherals/lcd/rgb_panel/README.md
//
// The prior implementation (ESP32 board version 2.x) was largely undocumented.

#include "Arduino_DataBus.h"

#if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)

#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"

#include "esp32s3/rom/cache.h"
// This function is located in ROM (also see esp_rom/${target}/ld/${target}.rom.ld)
extern int Cache_WriteBack_Addr(uint32_t addr, uint32_t size);

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error CamperUI RGB recovery requires Arduino ESP32 core 3.x
#endif

class CamperRGBPanel
{
public:
  CamperRGBPanel(
      int8_t de, int8_t vsync, int8_t hsync, int8_t pclk,
      int8_t r0, int8_t r1, int8_t r2, int8_t r3, int8_t r4,
      int8_t g0, int8_t g1, int8_t g2, int8_t g3, int8_t g4, int8_t g5,
      int8_t b0, int8_t b1, int8_t b2, int8_t b3, int8_t b4,
      uint16_t hsync_polarity, uint16_t hsync_front_porch, uint16_t hsync_pulse_width, uint16_t hsync_back_porch,
      uint16_t vsync_polarity, uint16_t vsync_front_porch, uint16_t vsync_pulse_width, uint16_t vsync_back_porch,
      uint16_t pclk_active_neg = 0, int32_t prefer_speed = GFX_NOT_DEFINED, bool useBigEndian = false,
      uint16_t de_idle_high = 0, uint16_t pclk_idle_high = 0);

  bool begin(int32_t speed = GFX_NOT_DEFINED);
  esp_err_t restartTransmission();

  bool isUseBigEndian() {
    return _useBigEndian;
  }

  uint16_t *getFrameBuffer(int16_t w, int16_t h);

protected:
private:
  int32_t _speed;
  int8_t _de, _vsync, _hsync, _pclk;
  int8_t _r0, _r1, _r2, _r3, _r4;
  int8_t _g0, _g1, _g2, _g3, _g4, _g5;
  int8_t _b0, _b1, _b2, _b3, _b4;
  uint16_t _hsync_polarity;
  uint16_t _hsync_front_porch;
  uint16_t _hsync_pulse_width;
  uint16_t _hsync_back_porch;
  uint16_t _vsync_polarity;
  uint16_t _vsync_front_porch;
  uint16_t _vsync_pulse_width;
  uint16_t _vsync_back_porch;
  uint16_t _pclk_active_neg;
  int32_t _prefer_speed;
  bool _useBigEndian;
  uint16_t _de_idle_high;
  uint16_t _pclk_idle_high;

  esp_lcd_panel_handle_t _panel_handle = NULL;
};

#endif // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)
