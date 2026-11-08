#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include "CamperRGBDisplay.h"
#include "display_sync.h"
#include "wifi_diagnostics.h"
#include "improv_serial.h"
#include "lv_conf.h"
#include "HWCDC.h"
#include "TouchDrvGT911.hpp"
#include <Wire.h>
#include "WS_CH32_IO.h"
#include "ui_main.h"
#include "http_handler.h"
#include "system_state.h"
#include "ui_screensaver.h"
#include "web_ota.h"
#include "buzzer.h"
#include "debug_log.h"
#include "temp_history.h"
#include "energy_history.h"
#include "victron_client.h"
#include "volumio_client.h"
#include "powerunit_client.h"
#include "touch_release_filter.h"

// Hardware and Touch Controller
TouchDrvGT911 GT911;
int16_t x[5], y[5];
uint8_t gt911_i2c_addr = 0;
bool gt911_available = false;
bool display_is_on = true;

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

uint32_t screenWidth;
uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;

// Display Bus & RGB Panel Configuration for Waveshare ESP32-S3-Touch-LCD-4 (480x480 ST7701S)
Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED /* DC */, 42 /* CS */,
    2 /* SCK */, 1 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

CamperRGBPanel *rgbpanel = new CamperRGBPanel(
    40 /* DE */, 39 /* VSYNC */, 38 /* HSYNC */, 41 /* PCLK */,
    46 /* R0 */, 3 /* R1 */, 8 /* R2 */, 18 /* R3 */, 17 /* R4 */,
    14 /* G0 */, 13 /* G1 */, 12 /* G2 */, 11 /* G3 */, 10 /* G4 */, 9 /* G5 */,
    5 /* B0 */, 45 /* B1 */, 48 /* B2 */, 47 /* B3 */, 21 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */,
    0 /* pclk_active_neg */, 12000000 /* prefer_speed: restore original panel timing */);

CamperRGBDisplay *gfx = new CamperRGBDisplay(
    480 /* width */, 480 /* height */, rgbpanel, 2 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

#if LV_USE_LOG != 0
void my_print(const char *buf) {
    USBSerial.printf("%s", buf);
}
#endif

// Display flushing callback
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp_drv);
}

void example_increase_lvgl_tick(void *arg) {
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

bool ignore_touch_until_release = false;
bool touch_input_enabled = true;
bool touch_is_pressed = false;
lv_point_t touch_last_point = {0, 0};

static void st7701_panel_sleep(bool sleep) {
    if (!bus) return;
    if (sleep) {
        bus->sendCommand(0x28); // Display Off
        delay(20);
        bus->sendCommand(0x10); // Sleep In
    } else {
        bus->sendCommand(0x11); // Sleep Out
        delay(20);
        bus->sendCommand(0x29); // Display On
    }
}

static lv_indev_t *touch_indev = nullptr;
static bool momentum_coasting = false;

// Read only fresh GT911 frames; retain a held touch between sensor updates.
void my_touchpad_read(lv_indev_drv_t *, lv_indev_data_t *data) {
    struct TouchSample {
        int16_t x;
        int16_t y;
        uint32_t ms;
    };
    static lv_point_t last_valid_point = {0, 0};
    static TouchSample hist[5] = {};
    static uint8_t hist_count = 0;
    static uint8_t stationary_fresh_cnt = 0;
    static uint32_t last_move_fresh_ms = 0;
    static uint32_t last_coast_ms = 0;
    static int32_t vel_x_q8 = 0;
    static int32_t vel_y_q8 = 0;
    static int32_t rem_x_q8 = 0;
    static int32_t rem_y_q8 = 0;
    static lv_dir_t saved_scroll_dir = LV_DIR_NONE;
    static bool was_held = false;
    static bool was_pressed = false;
    static TouchReleaseFilter release_filter;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) indev = touch_indev;

    data->point = last_valid_point;
    if (!gt911_available || !touch_input_enabled) {
        release_filter.reset();
        momentum_coasting = false;
        vel_x_q8 = 0;
        vel_y_q8 = 0;
        hist_count = 0;
        was_held = false;
        was_pressed = false;
        data->state = LV_INDEV_STATE_REL;
        touch_is_pressed = false;
        return;
    }
    uint32_t now = millis();
    // SensorLib getPoint clears 0x814E even when its data-ready bit is unset.
    // Inspect that bit first so an empty poll cannot fabricate finger lift-off.
    Wire.beginTransmission(gt911_i2c_addr);
    Wire.write(0x81); Wire.write(0x4e);
    bool status_ok = Wire.endTransmission(false) == 0;
    int status = -1;
    if (status_ok && Wire.requestFrom(gt911_i2c_addr, (uint8_t)1) == 1)
        status = Wire.read();
    bool fresh = status >= 0 && (status & 0x80);
    bool pressed = false;
    if (fresh) {
        uint8_t touched = GT911.getPoint(x, y, GT911.getSupportTouchPoint());
        uint8_t expected = status & 0x0f;
        if (touched != expected || touched > GT911.getSupportTouchPoint() ||
            (touched && (x[0] < 0 || y[0] < 0 || x[0] >= gfx->width() || y[0] >= gfx->height()))) {
            fresh = false; // Bad frames cannot move the pointer or create a click.
        } else if (touched) {
            int16_t tx = x[0], ty = y[0];
            switch (gfx->getRotation()) {
                case 1: tx = y[0]; ty = gfx->height() - 1 - x[0]; break;
                case 2: tx = gfx->width() - 1 - x[0]; ty = gfx->height() - 1 - y[0]; break;
                case 3: tx = gfx->width() - 1 - y[0]; ty = x[0]; break;
            }
            last_valid_point = {tx, ty}; data->point = last_valid_point;
            touch_last_point = last_valid_point;
            pressed = true;
            if (ui_screensaver_is_active()) {
                ui_screensaver_exit();
                display_is_on = true;
                ignore_touch_until_release = true;
                lv_disp_trig_activity(nullptr);
            } else if (!display_is_on) {
                display_is_on = true;
                ignore_touch_until_release = true;
                st7701_panel_sleep(false);
                WS_CH32_IO::setPwm(Wire, ui_screensaver_brightness_to_pwm(state.display_brightness));
                lv_disp_trig_activity(nullptr);
            }
        }
    }
    bool held = release_filter.update(fresh, pressed, now);
    if (!held) ignore_touch_until_release = false;

    if (momentum_coasting && (!indev || indev->proc.types.pointer.scroll_obj == nullptr)) {
        momentum_coasting = false;
        vel_x_q8 = 0;
        vel_y_q8 = 0;
    }

    // Tap-to-stop: if a list/page is actively coasting and the user touches the screen,
    // stop the momentum scroll immediately and swallow the click so no underlying control triggers.
    if (!was_held && held && !ignore_touch_until_release && momentum_coasting &&
        indev && indev->proc.types.pointer.scroll_obj != nullptr) {
        momentum_coasting = false;
        vel_x_q8 = 0;
        vel_y_q8 = 0;
        rem_x_q8 = 0;
        rem_y_q8 = 0;
        indev->proc.types.pointer.scroll_dir = saved_scroll_dir;
        indev->proc.types.pointer.scroll_throw_vect.x = 0;
        indev->proc.types.pointer.scroll_throw_vect.y = 0;
        indev->proc.types.pointer.scroll_throw_vect_ori.x = 0;
        indev->proc.types.pointer.scroll_throw_vect_ori.y = 0;
        ignore_touch_until_release = true;
        lv_disp_trig_activity(nullptr);
    }

    was_held = held;
    touch_is_pressed = held && !ignore_touch_until_release;
    data->state = touch_is_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

    if (touch_is_pressed) {
        if (!was_pressed) {
            hist[0] = {last_valid_point.x, last_valid_point.y, now};
            hist_count = 1;
            stationary_fresh_cnt = 0;
            last_move_fresh_ms = now;
            vel_x_q8 = 0;
            vel_y_q8 = 0;
            rem_x_q8 = 0;
            rem_y_q8 = 0;
            momentum_coasting = false;
        } else if (fresh && pressed) {
            TouchSample & prev = hist[hist_count - 1];
            int16_t dx = last_valid_point.x - prev.x;
            int16_t dy = last_valid_point.y - prev.y;
            if (dx != 0 || dy != 0) {
                if (hist_count < 5) {
                    hist[hist_count++] = {last_valid_point.x, last_valid_point.y, now};
                } else {
                    for (uint8_t i = 0; i < 4; ++i) hist[i] = hist[i + 1];
                    hist[4] = {last_valid_point.x, last_valid_point.y, now};
                }
                stationary_fresh_cnt = 0;
                last_move_fresh_ms = now;
            } else {
                stationary_fresh_cnt++;
                // Only clear swipe history if user deliberately held finger still for >= 6 fresh frames AND > 180ms
                if (stationary_fresh_cnt >= 6 && (now - last_move_fresh_ms > 180)) {
                    hist[0] = {last_valid_point.x, last_valid_point.y, now};
                    hist_count = 1;
                }
            }
        }
    } else if (was_pressed) {
        // Finger just released: compute flick velocity from fresh touch history
        lv_obj_t *scroll_obj = indev ? indev->proc.types.pointer.scroll_obj : nullptr;
        lv_dir_t dir = indev ? indev->proc.types.pointer.scroll_dir : LV_DIR_NONE;
        if (scroll_obj && lv_obj_has_flag(scroll_obj, LV_OBJ_FLAG_SCROLL_MOMENTUM) &&
            hist_count >= 2 && stationary_fresh_cnt < 6) {
            const TouchSample & newest = hist[hist_count - 1];
            int32_t best_vx_q8 = 0;
            int32_t best_vy_q8 = 0;
            for (int i = (int)hist_count - 2; i >= 0; --i) {
                uint32_t dt = newest.ms - hist[i].ms;
                if (dt == 0) dt = 8;
                if (dt > 220 && i < (int)hist_count - 2) break;
                uint32_t clamped_dt = dt < 8 ? 8 : (dt > 180 ? 180 : dt);
                int32_t vx = ((int32_t)(newest.x - hist[i].x) * 256 * 10) / (int32_t)clamped_dt;
                int32_t vy = ((int32_t)(newest.y - hist[i].y) * 256 * 10) / (int32_t)clamped_dt;
                if (abs(vx) > abs(best_vx_q8)) best_vx_q8 = vx;
                if (abs(vy) > abs(best_vy_q8)) best_vy_q8 = vy;
            }
            if (dir == LV_DIR_VER || (dir != LV_DIR_HOR && abs(best_vy_q8) >= abs(best_vx_q8))) {
                dir = LV_DIR_VER;
                vel_x_q8 = 0;
                vel_y_q8 = constrain((best_vy_q8 * 3) / 2, -48 * 256, 48 * 256);
            } else {
                dir = LV_DIR_HOR;
                vel_y_q8 = 0;
                vel_x_q8 = constrain((best_vx_q8 * 3) / 2, -48 * 256, 48 * 256);
            }
            if (abs(vel_x_q8) >= 256 || abs(vel_y_q8) >= 256) {
                momentum_coasting = true;
                saved_scroll_dir = dir;
                last_coast_ms = now;
                rem_x_q8 = 0;
                rem_y_q8 = 0;
            }
        }
        if (!momentum_coasting && indev) {
            vel_x_q8 = 0;
            vel_y_q8 = 0;
            indev->proc.types.pointer.scroll_throw_vect.x = 0;
            indev->proc.types.pointer.scroll_throw_vect.y = 0;
            indev->proc.types.pointer.scroll_throw_vect_ori.x = 0;
            indev->proc.types.pointer.scroll_throw_vect_ori.y = 0;
        }
    }

    if (momentum_coasting && !touch_is_pressed && indev) {
        lv_obj_t *scroll_obj = indev->proc.types.pointer.scroll_obj;
        if (!scroll_obj || !lv_obj_is_valid(scroll_obj)) {
            momentum_coasting = false;
            vel_x_q8 = 0;
            vel_y_q8 = 0;
        } else {
            uint32_t dt_coast = now - last_coast_ms;
            if (dt_coast == 0) dt_coast = 10;
            else if (dt_coast > 60) dt_coast = 60;
            last_coast_ms = now;

            if (saved_scroll_dir == LV_DIR_VER) {
                if ((vel_y_q8 > 0 && lv_obj_get_scroll_top(scroll_obj) <= 0) ||
                    (vel_y_q8 < 0 && lv_obj_get_scroll_bottom(scroll_obj) <= 0)) {
                    vel_y_q8 = 0;
                    rem_y_q8 = 0;
                }
            } else if (saved_scroll_dir == LV_DIR_HOR) {
                if ((vel_x_q8 > 0 && lv_obj_get_scroll_left(scroll_obj) <= 0) ||
                    (vel_x_q8 < 0 && lv_obj_get_scroll_right(scroll_obj) <= 0)) {
                    vel_x_q8 = 0;
                    rem_x_q8 = 0;
                }
            }

            // Frame-time-normalized exponential friction (~800ms glide regardless of page render time)
            int32_t decay = constrain((int32_t)(dt_coast * 10) / 10, 6, 56);
            vel_x_q8 = (vel_x_q8 * (256 - decay)) / 256;
            vel_y_q8 = (vel_y_q8 * (256 - decay)) / 256;

            if (abs(vel_x_q8) < 48 && abs(vel_y_q8) < 48) {
                momentum_coasting = false;
                vel_x_q8 = 0;
                vel_y_q8 = 0;
                rem_x_q8 = 0;
                rem_y_q8 = 0;
                indev->proc.types.pointer.scroll_dir = saved_scroll_dir;
                indev->proc.types.pointer.scroll_throw_vect.x = 0;
                indev->proc.types.pointer.scroll_throw_vect.y = 0;
                indev->proc.types.pointer.scroll_throw_vect_ori.x = 0;
                indev->proc.types.pointer.scroll_throw_vect_ori.y = 0;
            } else {
                rem_x_q8 += (vel_x_q8 * (int32_t)dt_coast) / 10;
                rem_y_q8 += (vel_y_q8 * (int32_t)dt_coast) / 10;
                int16_t step_x = (int16_t)(rem_x_q8 / 256);
                int16_t step_y = (int16_t)(rem_y_q8 / 256);
                rem_x_q8 -= (int32_t)step_x * 256;
                rem_y_q8 -= (int32_t)step_y * 256;

                if (step_x != 0 || step_y != 0) {
                    indev->proc.types.pointer.scroll_dir = saved_scroll_dir;
                    indev->proc.types.pointer.scroll_throw_vect.x = step_x;
                    indev->proc.types.pointer.scroll_throw_vect.y = step_y;
                    indev->proc.types.pointer.scroll_throw_vect_ori = indev->proc.types.pointer.scroll_throw_vect;
                } else {
                    // Sub-pixel tail tick: pause throw handler for 1 tick without ending scroll_obj
                    indev->proc.types.pointer.scroll_dir = LV_DIR_NONE;
                }
            }
        }
    }

    was_pressed = touch_is_pressed;
}

void i2c_scan() {
    USBSerial.println("Scanning I2C bus...");
    byte error, address;
    int nDevices = 0;

    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            USBSerial.print("I2C device found at 0x");
            if (address < 16) USBSerial.print("0");
            USBSerial.println(address, HEX);
            nDevices++;

            if (address == GT911_SLAVE_ADDRESS_L || address == GT911_SLAVE_ADDRESS_H) {
                gt911_i2c_addr = address;
                USBSerial.print("Found GT911 candidate: 0x");
                USBSerial.println(address, HEX);
            }
        }
    }
    if (nDevices == 0) {
        USBSerial.println("No I2C devices found");
    }
}

bool init_gt911_with_probe(int sda_pin, int scl_pin) {
    // Note: TwoWire was already initialized by WS_CH32_IO::begin() at 400kHz.
    // Avoid redundant Wire.begin() which would reset bus timing.
    delay(20);

    i2c_scan();

    if (gt911_i2c_addr == 0) {
        USBSerial.println("GT911 not found in I2C scan");
        return false;
    }

    GT911.setPins(-1, -1);
    if (GT911.begin(Wire, gt911_i2c_addr, sda_pin, scl_pin)) {
        USBSerial.print("GT911 initialized at 0x");
        USBSerial.println(gt911_i2c_addr, HEX);
        return true;
    } else {
        USBSerial.println("Failed to init GT911");
        return false;
    }
}

#include <WiFi.h>

void setup() {
    // 240 MHz full dual-core speed for maximum smooth LVGL UI rendering & touch responsiveness
    setCpuFrequencyMhz(240);

    ui_fonts_init();

    USBSerial.begin(115200);
    USBSerial.setTxTimeoutMs(0); // Never block if USB CDC host is not actively reading
    delay(200);
    USBSerial.println("\n--- Waveshare ESP32-S3 CamperUI Starting ---");
    USBSerial.printf("CPU frequency: %u MHz\n", (unsigned)getCpuFrequencyMhz());

    state_init();
    temp_history_init();
    energy_history_init();
    http_init();

    // Initialize CH32V003 IO expander (powers on LCD and resets GT911/ST7701S)
    if (!WS_CH32_IO::begin(Wire, WS_CH32_IO::DEFAULT_I2C_SDA, WS_CH32_IO::DEFAULT_I2C_SCL,
                           WS_CH32_IO::DEFAULT_I2C_FREQ, &USBSerial)) {
        USBSerial.println("CH32V003 IO expander init failed");
    } else {
        USBSerial.println("CH32V003 IO expander initialized");
        delay(20);
        USBSerial.printf("Setting PWM to %d\n", state.display_brightness);
        uint8_t pwm = ui_screensaver_brightness_to_pwm(state.display_brightness);
        WS_CH32_IO::setPwm(Wire, pwm);
        buzzer_init();
    }

    // Initialize Touch Controller GT911
    gt911_available = init_gt911_with_probe(15, 7);
    if (gt911_available) {
        GT911.setMaxTouchPoint(1);
    } else {
        USBSerial.println("GT911 touch not detected.");
    }

    // Initialize Display
    if (!gfx->begin()) {
        USBSerial.println("[DISPLAY " CAMPERUI_VERSION "] FATAL: RGB display initialization failed");
        while (true) delay(1000);
    }
    screenWidth = gfx->width();
    screenHeight = gfx->height();

    // Re-affirm backlight PWM now that the ST7701S panel is fully initialized
    {
        uint8_t pwm = ui_screensaver_brightness_to_pwm(state.display_brightness);
        WS_CH32_IO::setPwm(Wire, pwm);
    }

    // Initialize LVGL and allocate internal SRAM draw buffers BEFORE WiFi and
    // background tasks fragment internal heap, guaranteeing 2x 20-line double buffering.
    lv_init();

    const uint32_t internal_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    uint32_t buf_lines = 20;
    uint32_t buf_size = screenWidth * buf_lines;
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), internal_caps);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), internal_caps);
    if (!buf2 && buf1) {
        // Fall back to 16 lines if 20-line double-buffer didn't fit
        heap_caps_free(buf1);
        buf_lines = 16;
        buf_size = screenWidth * buf_lines;
        buf1 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), internal_caps);
        buf2 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), internal_caps);
    }
    if (!buf1) {
        USBSerial.println("[DISPLAY " CAMPERUI_VERSION "] FATAL: internal LVGL draw buffer allocation failed");
        while (true) delay(1000);
    }

    wifi_diagnostics_init();
    wifi_connect_configured();
    const bool data_worker_started = http_start_task();
    victron_init();
    radio_init();
    powerunit_init();
    USBSerial.printf("[DATA " CAMPERUI_VERSION "] Source=%s worker=%s\n",
                     state.debug_mode ? "DEMO" : "LIVE HTTP",
                     data_worker_started ? "RUNNING" : "FAILED");

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    if (buf2) {
        USBSerial.printf("[DISPLAY " CAMPERUI_VERSION "] LVGL double draw buffer enabled: 2x %u bytes (%u lines each), internal RAM only\n",
                         (unsigned)(buf_size * sizeof(lv_color_t)), (unsigned)buf_lines);
        lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);
    } else {
        USBSerial.printf("[DISPLAY " CAMPERUI_VERSION "] LVGL single draw buffer enabled: 1x %u bytes (%u lines), internal RAM only\n",
                         (unsigned)(buf_size * sizeof(lv_color_t)), (unsigned)buf_lines);
        lv_disp_draw_buf_init(&draw_buf, buf1, NULL, buf_size);
    }

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.sw_rotate = 0; // Handled directly in fast driver layer with 32-bit burst writes
    lv_disp_drv_register(&disp_drv);

    if (gt911_available) {
        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = my_touchpad_read;
        indev_drv.scroll_limit = 6; // 6px threshold balances crisp scrolling and reliable button taps
        indev_drv.scroll_throw = 0; // Handled by Q8 fixed-point momentum engine in my_touchpad_read
        touch_indev = lv_indev_drv_register(&indev_drv);
    }

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);



    ui_init();
    ui_screensaver_init();
    web_ota_init();
    display_request_resync();

    // Report memory after UI construction; the data worker started earlier.

    USBSerial.printf("[DISPLAY " CAMPERUI_VERSION "] Internal heap after startup: free=%u largest=%u bytes\n",
                     (unsigned)heap_caps_get_free_size(internal_caps),
                     (unsigned)heap_caps_get_largest_free_block(internal_caps));

    USBSerial.println("CamperUI ready!");
    USBSerial.println("[DISPLAY " CAMPERUI_VERSION "] Adaptive internal draw buffer active; f=framebuffer cache flush, s=RGB resync");
    USBSerial.println("[DISPLAY " CAMPERUI_VERSION "] RGB pixel clock: 12 MHz (original panel timing)");
    USBSerial.println("[TOUCH " CAMPERUI_VERSION "] n=geometry/memory, t=touch on/off, r=full screen redraw");
}

static uint32_t last_ui_update = 0;
static bool display_resync_pending = false;

void display_request_resync() {
    display_resync_pending = true;
}

void loop() {
    if (web_ota_is_updating()) {
        web_ota_loop();
        delay(1);
        return;
    }

    uint32_t wait_ms = lv_timer_handler();

    if (display_resync_pending) {
        display_resync_pending = false;
        // Flash writes have returned and cache access is available again.
        // The IDF driver applies the restart at the next VSYNC, not mid-frame.
        gfx->flush(true);
        esp_err_t result = rgbpanel->restartTransmission();
        USBSerial.printf("[DISPLAY " CAMPERUI_VERSION "] RGB resync scheduled after save/manual command: %s\n",
                         esp_err_to_name(result));
    }

    if (state.display_timeout > 0 && lv_disp_get_inactive_time(NULL) > (uint32_t)(state.display_timeout * 1000)) {
        if (state.screensaver_enabled) {
            if (!ui_screensaver_is_active()) {
                ui_screensaver_enter();
            }
        } else if (display_is_on) {
            display_is_on = false;
            WS_CH32_IO::setPwm(Wire, 255); // 255 is OFF for inverted PWM
            st7701_panel_sleep(true);
        }
    }

    while (USBSerial.available()) {
        char c = USBSerial.read();
        if (improv_serial_process_char(c)) {
            continue;
        }
        if (c == 't' || c == 'T') {
            touch_input_enabled = !touch_input_enabled;
            ignore_touch_until_release = true;
            lv_indev_reset(NULL, NULL);
            USBSerial.printf("[TOUCH " CAMPERUI_VERSION "] Touch %s (temporary, until reboot)\n",
                             touch_input_enabled ? "ON" : "OFF");
        }
        if (c == 's' || c == 'S') display_request_resync();
        if (c == 'f' || c == 'F') {
            gfx->flush(true);
            USBSerial.println("[DISPLAY " CAMPERUI_VERSION "] Framebuffer cache flush requested (no redraw)");
        }
        if (c == 'r' || c == 'R') {
            lv_obj_invalidate(lv_scr_act());
            USBSerial.println("[NAV " CAMPERUI_VERSION "] Full screen redraw requested");
        }
        if (c == 'n' || c == 'N') {
            ui_debug_navigation("serial", true);
            USBSerial.printf("[NAV " CAMPERUI_VERSION "] LVGL pool integrity: %s\n", lv_mem_test() == LV_RES_OK ? "OK" : "FAILED");
        }
        if (c == 'd' || c == 'D') {
            bool dbg;
            {
                StateLockGuard lock;
                state.debug_mode = !state.debug_mode;
                dbg = state.debug_mode;
            }
            state_save();
            USBSerial.printf("[COMMAND] Debug Simulation Mode: %s\n", dbg ? "AKTIV (Dummy-Daten)" : "INAKTIV (Live HTTP)");
        }
        if (c == 'w' || c == 'W') {
            String ssid;
            {
                StateLockGuard lock;
                ssid = state.wifi_ssid;
            }
            USBSerial.printf("[WIFI] Status: %d, SSID: '%s', IP: %s, RSSI: %d dBm | FreeInternal: %u, FreePSRAM: %u\n",
                             WiFi.status(), ssid.c_str(), WiFi.localIP().toString().c_str(),
                             WiFi.RSSI(),
                             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        }
        if (c == 'b' || c == 'B') {
            buzzer_beep(50);
            USBSerial.println("[BUZZER] Test-Beep (50ms)");
        }
        if (c == 'k' || c == 'K') {
            if (ui_screensaver_is_active()) {
                ui_screensaver_exit();
                USBSerial.println("[SCREENSAVER] Manual Exit");
            } else {
                ui_screensaver_enter();
                USBSerial.println("[SCREENSAVER] Manual Enter");
            }
        }
        if (c == 'p' || c == 'P') {
            int val = USBSerial.parseInt();
            WS_CH32_IO::setPwm(Wire, val);
            USBSerial.printf("[PWM] Set raw PWM to %d (duty %d%%)\n", val, (255 - val) * 100 / 255);
        }
    }

    buzzer_loop();
    temp_history_tick();
    energy_history_tick();
    victron_tick();

    // Update UI dynamically
    static uint32_t last_ui_ms = 0;
    static uint32_t last_level_ui_ms = 0;
    uint32_t now_ui = millis();
    if (display_is_on && now_ui - last_ui_ms > 1000) {
        last_ui_ms = now_ui;
        last_level_ui_ms = now_ui;
        {
            StateLockGuard lock;
            if (ui_screensaver_is_active()) {
                ui_screensaver_update();
            } else {
                ui_update_data();
                ui_update_visible_page();
                // Complete asynchronous WLAN scans even after leaving Settings.
                ui_update_settings_tab();
            }
        }
    } else if (display_is_on && !ui_screensaver_is_active() && ui_is_level_tab_active() && (now_ui - last_level_ui_ms >= 250)) {
        last_level_ui_ms = now_ui;
        StateLockGuard lock;
        ui_update_level_tab();
    }
    
    web_ota_loop();
    wifi_diagnostics_loop();
    bool high_fps_needed = (lv_anim_count_running() > 0) || touch_is_pressed || momentum_coasting;
    if (!display_is_on) {
        delay(40);
    } else if (ui_screensaver_is_active() && !touch_is_pressed) {
        uint32_t sleep_ms = wait_ms > 35 ? 35 : (wait_ms > 0 ? wait_ms : 10);
        delay(sleep_ms);
    } else if (wait_ms > 0 && !high_fps_needed) {
        uint32_t max_sleep = (lv_disp_get_inactive_time(NULL) > 3000) ? 15 : 5;
        if (wait_ms > max_sleep) wait_ms = max_sleep;
        delay(wait_ms);
    } else {
        yield();
    }
}
