#include "ui_screensaver.h"
#include "ui_main.h"
#include "system_state.h"
#include "WS_CH32_IO.h"
#include "ui_fonts.h"
#include <Wire.h>
#include <time.h>
#include "debug_log.h"

static lv_obj_t *screensaver_cont = nullptr;
static lv_obj_t *lbl_time = nullptr;
static lv_obj_t *lbl_ttgo = nullptr;
static lv_obj_t *lbl_sub = nullptr;
static bool screensaver_active = false;

uint8_t ui_screensaver_brightness_to_pwm(int brightness_pct) {
    int b = constrain(brightness_pct, 1, 100);
    // ST7701S backlight on CH32V003 IO expander:
    // Inverted PWM (255 = 0% duty / off, 0 = 100% duty / full brightness).
    // Hardware boost converter cuts out below ~10% duty (PWM > 230), turning backlight off.
    // Use a perceptual curve (50% linear + 50% quadratic) across [230 .. 0] so mid-high
    // settings save significantly more current without losing perceived contrast.
    uint32_t norm = (uint32_t)(b - 1); // 0..99
    uint32_t lin_part = norm * 99U;    // 0..9801
    uint32_t quad_part = norm * norm;  // 0..9801
    uint32_t curved = (lin_part + quad_part) / 2U; // 0..9801
    return (uint8_t)(230U - (curved * 230U / 9801U));
}

void ui_screensaver_init() {
    if (screensaver_cont) return;

    // Build screensaver overlay on LVGL system layer (sits above all screens)
    screensaver_cont = lv_obj_create(lv_layer_sys());
    lv_obj_set_size(screensaver_cont, 480, 480);
    lv_obj_set_pos(screensaver_cont, 0, 0);
    lv_obj_set_style_bg_color(screensaver_cont, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screensaver_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screensaver_cont, 0, 0);
    lv_obj_set_style_radius(screensaver_cont, 0, 0);
    lv_obj_set_style_pad_all(screensaver_cont, 0, 0);
    lv_obj_clear_flag(screensaver_cont, LV_OBJ_FLAG_SCROLLABLE);

    // Flex centered content container
    lv_obj_t *box = lv_obj_create(screensaver_cont);
    lv_obj_set_size(box, 460, 320);
    lv_obj_center(box);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 8, 0);

    // 120px Clock in dark red (#dc2626)
    lbl_time = lv_label_create(box);
    lv_obj_set_style_text_font(lbl_time, &ui_font_clock_120, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(0xdc2626), 0);
    lv_label_set_text(lbl_time, "--:--");

    // Time to go: large 28px font, matching color (#dc2626)
    lbl_ttgo = lv_label_create(box);
    lv_obj_set_style_text_font(lbl_ttgo, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_ttgo, lv_color_hex(0xdc2626), 0);
    lv_label_set_text(lbl_ttgo, "Time to go: --");

    // Battery / Voltage status subtitle: 20px font, matching color (#dc2626)
    lbl_sub = lv_label_create(box);
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_sub, lv_color_hex(0xdc2626), 0);
    lv_label_set_text(lbl_sub, "");

    // Safety touch handler on container
    lv_obj_add_event_cb(screensaver_cont, [](lv_event_t *) {
        ui_screensaver_exit();
    }, LV_EVENT_PRESSED, nullptr);

    // Initially hidden
    lv_obj_add_flag(screensaver_cont, LV_OBJ_FLAG_HIDDEN);
}

bool ui_screensaver_is_active() {
    return screensaver_active;
}

void ui_screensaver_update() {
    if (!screensaver_cont) return;

    time_t now = time(nullptr);
    struct tm ti;
    if (now > 100000 && localtime_r(&now, &ti)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        ui_label_set_text_if_changed(lbl_time, buf);
    } else {
        ui_label_set_text_if_changed(lbl_time, "--:--");
    }

    if (state.time_to_go.length() > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Time to go: %s", state.time_to_go.c_str());
        ui_label_set_text_if_changed(lbl_ttgo, buf);
    } else {
        ui_label_set_text_if_changed(lbl_ttgo, "Time to go: --");
    }

    if (lbl_sub) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Batterie: %d%% | %.1f V", state.bat_soc, state.bat_voltage);
        ui_label_set_text_if_changed(lbl_sub, buf);
    }
}

void ui_screensaver_enter() {
    if (screensaver_active) return;
    screensaver_active = true;

    if (screensaver_cont) {
        ui_screensaver_update();
        lv_obj_clear_flag(screensaver_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(screensaver_cont);
        lv_refr_now(nullptr);
    }

    // Dim backlight to user-configured screensaver brightness mapped to hardware range
    uint8_t pwm = ui_screensaver_brightness_to_pwm(state.screensaver_brightness);
    WS_CH32_IO::setPwm(Wire, pwm);
    USBSerial.printf("[SCREENSAVER] Activated (backlight %d%% / PWM %d)\n", state.screensaver_brightness, pwm);
}

void ui_screensaver_exit() {
    if (!screensaver_active) return;
    screensaver_active = false;

    if (screensaver_cont) {
        lv_obj_add_flag(screensaver_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(nullptr);
    }

    // Restore user-configured brightness only after the screen has redrawn without screensaver
    uint8_t pwm = ui_screensaver_brightness_to_pwm(state.display_brightness);
    WS_CH32_IO::setPwm(Wire, pwm);
    lv_disp_trig_activity(nullptr);
    USBSerial.println("[SCREENSAVER] Deactivated (brightness restored)");
}
