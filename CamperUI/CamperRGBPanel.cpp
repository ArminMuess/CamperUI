// Project-local adaptation of Arduino_GFX 1.5.3 (Moon On Our Nation).
// See Arduino_GFX-license.txt for the retained BSD license.
// ESP_LCD_Panel implementation for esp32s3.

#include "CamperRGBPanel.h"

#if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)
CamperRGBPanel::CamperRGBPanel(
    int8_t de, int8_t vsync, int8_t hsync, int8_t pclk,
    int8_t r0, int8_t r1, int8_t r2, int8_t r3, int8_t r4,
    int8_t g0, int8_t g1, int8_t g2, int8_t g3, int8_t g4, int8_t g5,
    int8_t b0, int8_t b1, int8_t b2, int8_t b3, int8_t b4,
    uint16_t hsync_polarity, uint16_t hsync_front_porch, uint16_t hsync_pulse_width, uint16_t hsync_back_porch,
    uint16_t vsync_polarity, uint16_t vsync_front_porch, uint16_t vsync_pulse_width, uint16_t vsync_back_porch,
    uint16_t pclk_active_neg, int32_t prefer_speed, bool useBigEndian,
    uint16_t de_idle_high, uint16_t pclk_idle_high)
    : _de(de), _vsync(vsync), _hsync(hsync), _pclk(pclk),
      _r0(r0), _r1(r1), _r2(r2), _r3(r3), _r4(r4),
      _g0(g0), _g1(g1), _g2(g2), _g3(g3), _g4(g4), _g5(g5),
      _b0(b0), _b1(b1), _b2(b2), _b3(b3), _b4(b4),
      _hsync_polarity(hsync_polarity), _hsync_front_porch(hsync_front_porch), _hsync_pulse_width(hsync_pulse_width), _hsync_back_porch(hsync_back_porch),
      _vsync_polarity(vsync_polarity), _vsync_front_porch(vsync_front_porch), _vsync_pulse_width(vsync_pulse_width), _vsync_back_porch(vsync_back_porch),
      _pclk_active_neg(pclk_active_neg), _prefer_speed(prefer_speed), _useBigEndian(useBigEndian),
      _de_idle_high(de_idle_high), _pclk_idle_high(pclk_idle_high)
{
}

bool CamperRGBPanel::begin(int32_t speed)
{
  if (speed == GFX_NOT_DEFINED)
  {
#ifdef CONFIG_SPIRAM_MODE_QUAD
    _speed = 6000000L;
#else
    _speed = 12000000L;
#endif
  }
  else
  {
    _speed = speed;
  }

  return true;
}

#if (defined(ESP_ARDUINO_VERSION_MAJOR)) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
static volatile bool s_force_bb_resync = false;

// In ESP-IDF 5.x (Arduino ESP32 3.3.x), rgb_lcd_default_isr_handler resets GDMA to
// bounce_buffer[0] on VSYNC without resetting bb_eof_count (offset 0x128) or forcing
// bounce_pos_px (offset 0x124) to 0 when bounce_pos_px == 9600 (1 missed EOF).
// If any Flash/PSRAM/IRQ stall ever delays a single GDMA bounce-buffer EOF interrupt,
// bb_eof_count becomes odd at VSYNC, permanently swapping bounce_buffer[0] and [1]
// (which shifts the entire display down by 20-40 lines with bottom wrapping to top).
// This VSYNC callback runs inside rgb_lcd_default_isr_handler right before its restart
// check and guarantees bounce_pos_px and bb_eof_count stay in 100% lockstep with VSYNC.
static bool IRAM_ATTR rgb_vsync_bounce_guard_cb(esp_lcd_panel_handle_t panel,
                                                const esp_lcd_rgb_panel_event_data_t *edata,
                                                void *user_ctx)
{
  (void)edata;
  (void)user_ctx;
  uint8_t *raw = (uint8_t *)panel;
  uint32_t bb_bytes = *(volatile uint32_t *)(raw + 0x88); // bounce_buffer_size_bytes (19200)
  if (bb_bytes > 0)
  {
    volatile uint32_t *p_bounce_pos_px = (volatile uint32_t *)(raw + 0x124);
    volatile uint32_t *p_bb_eof_count  = (volatile uint32_t *)(raw + 0x128);
    // 2 bounce buffers of (bb_bytes / 2) pixels = bb_bytes pixels (19200) pre-filled at VSYNC
    if (s_force_bb_resync || *p_bounce_pos_px != bb_bytes || (*p_bb_eof_count & 1U) != 0U)
    {
      s_force_bb_resync = false;
      *p_bounce_pos_px = 0;
      *p_bb_eof_count = 0;
    }
    else
    {
      *p_bb_eof_count = 0;
    }
  }
  return false;
}
#endif

uint16_t *CamperRGBPanel::getFrameBuffer(int16_t w, int16_t h)
{
  esp_lcd_rgb_panel_config_t panel_config = {
#if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR < 3)
      .clk_src = LCD_CLK_SRC_PLL160M,
#else
      .clk_src = LCD_CLK_SRC_DEFAULT,
#endif
      .timings = {
          .pclk_hz = (uint32_t)((_prefer_speed == GFX_NOT_DEFINED) ? _speed : _prefer_speed),
          .h_res = (uint32_t)w,
          .v_res = (uint32_t)h,
          .hsync_pulse_width = _hsync_pulse_width,
          .hsync_back_porch = _hsync_back_porch,
          .hsync_front_porch = _hsync_front_porch,
          .vsync_pulse_width = _vsync_pulse_width,
          .vsync_back_porch = _vsync_back_porch,
          .vsync_front_porch = _vsync_front_porch,
          .flags = {
              .hsync_idle_low = (uint32_t)((_hsync_polarity == 0) ? 1 : 0),
              .vsync_idle_low = (uint32_t)((_vsync_polarity == 0) ? 1 : 0),
              .de_idle_high = _de_idle_high,
              .pclk_active_neg = _pclk_active_neg,
              .pclk_idle_high = _pclk_idle_high,
          },
      },
      .data_width = 16, // RGB565 in parallel mode, thus 16 bits in width
#if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR < 3)
#else
      .bits_per_pixel = 16,
      .num_fbs = 1,
      .bounce_buffer_size_px = 20 * w,
#endif
      .sram_trans_align = 8,
      .psram_trans_align = 64,
      .hsync_gpio_num = _hsync,
      .vsync_gpio_num = _vsync,
      .de_gpio_num = _de,
      .pclk_gpio_num = _pclk,
      .disp_gpio_num = GPIO_NUM_NC, // -1
      .data_gpio_nums = {0},
      .flags = {
          .disp_active_low = true,
          .refresh_on_demand = false,
          .fb_in_psram = true, // allocate frame buffer from PSRAM
          .double_fb = false,
          .no_fb = false,
          .bb_invalidate_cache = false,
      },
  };

  if (_useBigEndian)
  {
    panel_config.data_gpio_nums[0] = _g3;
    panel_config.data_gpio_nums[1] = _g4;
    panel_config.data_gpio_nums[2] = _g5;
    panel_config.data_gpio_nums[3] = _r0;
    panel_config.data_gpio_nums[4] = _r1;
    panel_config.data_gpio_nums[5] = _r2;
    panel_config.data_gpio_nums[6] = _r3;
    panel_config.data_gpio_nums[7] = _r4;
    panel_config.data_gpio_nums[8] = _b0;
    panel_config.data_gpio_nums[9] = _b1;
    panel_config.data_gpio_nums[10] = _b2;
    panel_config.data_gpio_nums[11] = _b3;
    panel_config.data_gpio_nums[12] = _b4;
    panel_config.data_gpio_nums[13] = _g0;
    panel_config.data_gpio_nums[14] = _g1;
    panel_config.data_gpio_nums[15] = _g2;
  }
  else
  {
    panel_config.data_gpio_nums[0] = _b0;
    panel_config.data_gpio_nums[1] = _b1;
    panel_config.data_gpio_nums[2] = _b2;
    panel_config.data_gpio_nums[3] = _b3;
    panel_config.data_gpio_nums[4] = _b4;
    panel_config.data_gpio_nums[5] = _g0;
    panel_config.data_gpio_nums[6] = _g1;
    panel_config.data_gpio_nums[7] = _g2;
    panel_config.data_gpio_nums[8] = _g3;
    panel_config.data_gpio_nums[9] = _g4;
    panel_config.data_gpio_nums[10] = _g5;
    panel_config.data_gpio_nums[11] = _r0;
    panel_config.data_gpio_nums[12] = _r1;
    panel_config.data_gpio_nums[13] = _r2;
    panel_config.data_gpio_nums[14] = _r3;
    panel_config.data_gpio_nums[15] = _r4;
  }

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &_panel_handle));

#if (defined(ESP_ARDUINO_VERSION_MAJOR)) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = rgb_vsync_bounce_guard_cb;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(_panel_handle, &cbs, this));
#endif

  ESP_ERROR_CHECK(esp_lcd_panel_reset(_panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(_panel_handle));

#if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR < 3)
  esp_rgb_panel_t *_rgb_panel;
  _rgb_panel = __containerof(_panel_handle, esp_rgb_panel_t, base);

  return (uint16_t *)_rgb_panel->fb;
#else
  void *frame_buffer = nullptr;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(_panel_handle, 1, &frame_buffer));

  return ((uint16_t *)frame_buffer);
#endif
}

#endif // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)

#if defined(ESP32) && CONFIG_IDF_TARGET_ESP32S3
esp_err_t CamperRGBPanel::restartTransmission() {
  if (!_panel_handle) return ESP_ERR_INVALID_STATE;
#if (defined(ESP_ARDUINO_VERSION_MAJOR)) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  s_force_bb_resync = true;
#endif
  return esp_lcd_rgb_panel_restart(_panel_handle);
}
#endif
