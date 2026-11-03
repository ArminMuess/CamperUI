#include "ui_main.h"
#include "http_handler.h"
#include <math.h>

static lv_obj_t *card_level_main;
static lv_obj_t *bubble_container;
static lv_obj_t *bubble_obj;
static lv_obj_t *lbl_level_status;

static lv_obj_t *lbl_pitch_val;
static lv_obj_t *lbl_roll_val;

// 4 Wheel compensation labels
static lv_obj_t *lbl_wheel_fl; // Front Left
static lv_obj_t *lbl_wheel_fr; // Front Right
static lv_obj_t *lbl_wheel_rl; // Rear Left
static lv_obj_t *lbl_wheel_rr; // Rear Right

static lv_obj_t *btn_calibrate;
static lv_obj_t *lbl_cal_msg;

// ==========================================
// Level Bubble Animation (Roll from Center)
// ==========================================
static bool level_animating = false;
static float anim_target_dx = 0.0f;
static float anim_target_dy = 0.0f;
static int last_is_level = -1;
static int16_t last_bubble_dx = 9999;
static int16_t last_bubble_dy = 9999;

static void level_anim_cb(void *var, int32_t val) {
    (void)var;
    if (!bubble_obj) return;
    float f = (float)val / 1000.0f;
    int16_t cur_x = (int16_t)(anim_target_dx * f);
    int16_t cur_y = (int16_t)(anim_target_dy * f);
    lv_obj_align(bubble_obj, LV_ALIGN_CENTER, cur_x, cur_y);
}

static void level_anim_ready_cb(lv_anim_t *a) {
    (void)a;
    level_animating = false;
    ui_update_level_tab();
}

void ui_trigger_level_anim() {
    lv_anim_del(NULL, level_anim_cb);
    if (!bubble_obj) {
        level_animating = false;
        return;
    }

    // Place bubble at center (0, 0) initially
    lv_obj_align(bubble_obj, LV_ALIGN_CENTER, 0, 0);

    float p = state.pitch_angle;
    float r = state.roll_angle;
    float dx = -r * 14.0f;
    float dy =  p * 14.0f;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist > 62.0f) {
        dx = (dx / dist) * 62.0f;
        dy = (dy / dist) * 62.0f;
    }
    anim_target_dx = dx;
    anim_target_dy = dy;

    if (fabsf(dx) < 0.5f && fabsf(dy) < 0.5f) {
        level_animating = false;
        return;
    }

    level_animating = true;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, NULL);
    lv_anim_set_exec_cb(&a, level_anim_cb);
    lv_anim_set_time(&a, 700);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_ready_cb(&a, level_anim_ready_cb);
    lv_anim_start(&a);
}

static void calibrate_click_cb(lv_event_t * e) {
    if (level_animating) {
        lv_anim_del(NULL, level_anim_cb);
        level_animating = false;
    }
    http_calibrate_position();
    if (lbl_cal_msg) {
        lv_label_set_text(lbl_cal_msg, "Nullpunkt an VanPi gesendet!");
        lv_obj_clear_flag(lbl_cal_msg, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_level_reset() {
    lv_anim_del(NULL, level_anim_cb);
    level_animating = false;
    card_level_main = nullptr;
    bubble_container = nullptr;
    bubble_obj = nullptr;
    lbl_level_status = nullptr;
    lbl_pitch_val = nullptr;
    lbl_roll_val = nullptr;
    lbl_wheel_fl = nullptr;
    lbl_wheel_fr = nullptr;
    lbl_wheel_rl = nullptr;
    lbl_wheel_rr = nullptr;
    btn_calibrate = nullptr;
    lbl_cal_msg = nullptr;
    last_is_level = -1;
    last_bubble_dx = 9999;
    last_bubble_dy = 9999;
}

void ui_build_level(lv_obj_t *parent) {
    ui_level_reset();

    ui_setup_tab_page(parent);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // =========================================================================
    // Main Card (440 x 360 px)
    // =========================================================================
    card_level_main = ui_create_card(parent, 440, 360);
    lv_obj_align(card_level_main, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(card_level_main, 12, 0);

    // Title
    lv_obj_t *lbl_title = lv_label_create(card_level_main);
    lv_label_set_text_fmt(lbl_title, "%s Wasserwaage und Keil-Assistent", LV_SYMBOL_DRIVE);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_title, ui_theme_text(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 4, 0);

    // =========================================================================
    // Left Side: 2D Circular Bubble Level (Dosenlibelle)
    // =========================================================================
    // Outer circular container: 180x180 px
    bubble_container = lv_obj_create(card_level_main);
    lv_obj_set_size(bubble_container, 180, 180);
    lv_obj_set_style_radius(bubble_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bubble_container, state.dark_mode ? lv_color_hex(0x13171f) : lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_border_width(bubble_container, 3, 0);
    lv_obj_set_style_border_color(bubble_container, ui_theme_border(), 0);
    lv_obj_align(bubble_container, LV_ALIGN_TOP_LEFT, 8, 36);
    lv_obj_clear_flag(bubble_container, LV_OBJ_FLAG_SCROLLABLE);

    // Crosshair Lines
    lv_obj_t *line_h = lv_obj_create(bubble_container);
    lv_obj_set_size(line_h, 174, 1);
    lv_obj_set_style_bg_color(line_h, ui_theme_border(), 0);
    lv_obj_set_style_border_width(line_h, 0, 0);
    lv_obj_center(line_h);

    lv_obj_t *line_v = lv_obj_create(bubble_container);
    lv_obj_set_size(line_v, 1, 174);
    lv_obj_set_style_bg_color(line_v, ui_theme_border(), 0);
    lv_obj_set_style_border_width(line_v, 0, 0);
    lv_obj_center(line_v);

    // Inner Target Circle: 46x46 px (corresponds to ~0.5 degree tolerance zone)
    lv_obj_t *target_ring = lv_obj_create(bubble_container);
    lv_obj_set_size(target_ring, 46, 46);
    lv_obj_set_style_radius(target_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(target_ring, LV_OPA_0, 0);
    lv_obj_set_style_border_width(target_ring, 2, 0);
    lv_obj_set_style_border_color(target_ring, lv_color_hex(UI_COLOR_SUCCESS), 0);
    lv_obj_center(target_ring);

    // The Moving Bubble
    bubble_obj = lv_obj_create(bubble_container);
    lv_obj_set_size(bubble_obj, 30, 30);
    lv_obj_set_style_radius(bubble_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bubble_obj, ui_theme_primary(), 0);
    lv_obj_set_style_border_width(bubble_obj, 2, 0);
    lv_obj_set_style_border_color(bubble_obj, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_shadow_width(bubble_obj, 0, 0);
    lv_obj_center(bubble_obj);

    // Level status label under the bubble container
    lbl_level_status = lv_label_create(card_level_main);
    lv_obj_set_style_text_font(lbl_level_status, &lv_font_montserrat_14, 0);
    lv_label_set_text(lbl_level_status, "Berechne...");
    lv_obj_set_style_text_align(lbl_level_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_level_status, LV_ALIGN_TOP_LEFT, 18, 224);
    lv_obj_set_width(lbl_level_status, 160);

    // =========================================================================
    // Right Side: 4-Wheel Wedge Assistant & Angles
    // =========================================================================
    lv_obj_t *lbl_pitch_title = lv_label_create(card_level_main);
    lv_label_set_text(lbl_pitch_title, "Neigung Laengs:");
    lv_obj_set_style_text_font(lbl_pitch_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_pitch_title, ui_theme_muted(), 0);
    lv_obj_align(lbl_pitch_title, LV_ALIGN_TOP_LEFT, 206, 36);

    lbl_pitch_val = lv_label_create(card_level_main);
    lv_label_set_text(lbl_pitch_val, "+0.0 deg");
    lv_obj_set_style_text_font(lbl_pitch_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_pitch_val, ui_theme_text(), 0);
    lv_obj_align(lbl_pitch_val, LV_ALIGN_TOP_LEFT, 335, 36);

    lv_obj_t *lbl_roll_title = lv_label_create(card_level_main);
    lv_label_set_text(lbl_roll_title, "Neigung Quer:");
    lv_obj_set_style_text_font(lbl_roll_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_roll_title, ui_theme_muted(), 0);
    lv_obj_align(lbl_roll_title, LV_ALIGN_TOP_LEFT, 206, 60);

    lbl_roll_val = lv_label_create(card_level_main);
    lv_label_set_text(lbl_roll_val, "+0.0 deg");
    lv_obj_set_style_text_font(lbl_roll_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_roll_val, ui_theme_text(), 0);
    lv_obj_align(lbl_roll_val, LV_ALIGN_TOP_LEFT, 335, 60);

    // 4-Wheel Graphic Box (stylized van top-view)
    lv_obj_t *van_box = lv_obj_create(card_level_main);
    lv_obj_set_size(van_box, 204, 140);
    lv_obj_set_style_radius(van_box, 10, 0);
    lv_obj_set_style_bg_color(van_box, state.dark_mode ? lv_color_hex(0x181c24) : lv_color_hex(0xf8fafc), 0);
    lv_obj_set_style_border_width(van_box, 1, 0);
    lv_obj_set_style_border_color(van_box, ui_theme_border(), 0);
    lv_obj_set_style_pad_all(van_box, 0, 0);
    lv_obj_align(van_box, LV_ALIGN_TOP_LEFT, 206, 88);
    lv_obj_clear_flag(van_box, LV_OBJ_FLAG_SCROLLABLE);

    auto create_wheel = [van_box](const char *pos, int x, int y) -> lv_obj_t* {
        lv_obj_t *c = lv_obj_create(van_box);
        lv_obj_set_size(c, 90, 58);
        lv_obj_set_style_radius(c, 8, 0);
        lv_obj_set_style_bg_color(c, ui_theme_card(), 0);
        lv_obj_set_style_border_width(c, 1, 0);
        lv_obj_set_style_border_color(c, ui_theme_border(), 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_align(c, LV_ALIGN_TOP_LEFT, x, y);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lt = lv_label_create(c);
        lv_label_set_text(lt, pos);
        lv_obj_set_style_text_font(lt, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lt, ui_theme_muted(), 0);
        lv_obj_set_width(lt, 90);
        lv_obj_set_style_text_align(lt, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lt, LV_ALIGN_TOP_MID, 0, 6);

        lv_obj_t *lv = lv_label_create(c);
        lv_label_set_text(lv, "OK");
        lv_obj_set_style_text_font(lv, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lv, lv_color_hex(UI_COLOR_SUCCESS), 0);
        lv_obj_set_width(lv, 90);
        lv_obj_set_style_text_align(lv, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lv, LV_ALIGN_BOTTOM_MID, 0, -8);
        return lv;
    };

    lbl_wheel_fl = create_wheel("Vorne L", 8, 8);
    lbl_wheel_fr = create_wheel("Vorne R", 106, 8);
    lbl_wheel_rl = create_wheel("Hinten L", 8, 74);
    lbl_wheel_rr = create_wheel("Hinten R", 106, 74);

    // =========================================================================
    // Bottom Action: Tare / Calibrate Button & Confirmation
    // =========================================================================
    btn_calibrate = lv_btn_create(card_level_main);
    lv_obj_set_size(btn_calibrate, 210, 44);
    lv_obj_align(btn_calibrate, LV_ALIGN_BOTTOM_LEFT, 8, -6);
    lv_obj_set_style_radius(btn_calibrate, ui_radius_button(), 0);
    lv_obj_set_style_bg_color(btn_calibrate, ui_theme_primary(), 0);
    lv_obj_add_event_cb(btn_calibrate, calibrate_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_btn = lv_label_create(btn_calibrate);
    lv_label_set_text_fmt(lbl_btn, "%s Nullpunkt Tara", LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(lbl_btn, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_btn, ui_theme_btn_active_text(), 0);
    lv_obj_center(lbl_btn);

    lbl_cal_msg = lv_label_create(card_level_main);
    lv_label_set_text(lbl_cal_msg, "");
    lv_obj_set_style_text_font(lbl_cal_msg, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_cal_msg, lv_color_hex(UI_COLOR_SUCCESS), 0);
    lv_obj_align(lbl_cal_msg, LV_ALIGN_BOTTOM_LEFT, 230, -18);
    lv_obj_add_flag(lbl_cal_msg, LV_OBJ_FLAG_HIDDEN);
}

void ui_update_level_tab() {
    if (!bubble_obj) return;

    float p = state.pitch_angle; // Front/Rear tilt in degrees
    float r = state.roll_angle;  // Left/Right tilt in degrees

    // Update digital readouts
    ui_label_set_float(lbl_pitch_val, "%+.1f deg", p);
    ui_label_set_float(lbl_roll_val, "%+.1f deg", r);

    // Calculate bubble offset (scale: 1 degree ~ 14 pixels, clamped to 62px radius)
    float dx = -r * 14.0f;
    float dy =  p * 14.0f;

    float dist = sqrtf(dx * dx + dy * dy);
    if (dist > 62.0f) {
        dx = (dx / dist) * 62.0f;
        dy = (dy / dist) * 62.0f;
    }

    if (!level_animating) {
        int16_t idx = (int16_t)dx;
        int16_t idy = (int16_t)dy;
        if (idx != last_bubble_dx || idy != last_bubble_dy) {
            last_bubble_dx = idx;
            last_bubble_dy = idy;
            lv_obj_align(bubble_obj, LV_ALIGN_CENTER, idx, idy);
        }
    }

    bool is_level = (fabsf(p) <= 0.5f && fabsf(r) <= 0.5f);
    if ((int)is_level != last_is_level) {
        last_is_level = (int)is_level;
        if (is_level) {
            lv_obj_set_style_bg_color(bubble_obj, lv_color_hex(UI_COLOR_SUCCESS), 0);
            ui_label_set_text_if_changed(lbl_level_status, "Perfekt im Blei!");
            ui_text_color_if_changed(lbl_level_status, lv_color_hex(UI_COLOR_SUCCESS));
        } else {
            lv_obj_set_style_bg_color(bubble_obj, lv_color_hex(UI_COLOR_WARNING), 0);
            ui_label_set_text_if_changed(lbl_level_status, "Ausrichten nötig");
            ui_text_color_if_changed(lbl_level_status, lv_color_hex(UI_COLOR_WARNING));
        }
    }

    // Wheel compensation calculation
    float p_rad = p * 0.01745329f;
    float r_rad = r * 0.01745329f;

    float z_fl =  175.0f * sinf(p_rad) + 90.0f * sinf(r_rad);
    float z_fr =  175.0f * sinf(p_rad) - 90.0f * sinf(r_rad);
    float z_rl = -175.0f * sinf(p_rad) + 90.0f * sinf(r_rad);
    float z_rr = -175.0f * sinf(p_rad) - 90.0f * sinf(r_rad);

    float z_min = z_fl;
    if (z_fr < z_min) z_min = z_fr;
    if (z_rl < z_min) z_rl = z_rl;
    if (z_rr < z_min) z_rr = z_rr;

    auto update_wheel_label = [](lv_obj_t *lbl, float diff_cm) {
        if (!lbl) return;
        if (diff_cm < 1.0f) {
            ui_label_set_text_if_changed(lbl, "OK");
            ui_text_color_if_changed(lbl, lv_color_hex(UI_COLOR_SUCCESS));
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), "+%d cm", (int)(diff_cm + 0.5f));
            ui_label_set_text_if_changed(lbl, buf);
            ui_text_color_if_changed(lbl, lv_color_hex(UI_COLOR_WARNING));
        }
    };

    update_wheel_label(lbl_wheel_fl, z_fl - z_min);
    update_wheel_label(lbl_wheel_fr, z_fr - z_min);
    update_wheel_label(lbl_wheel_rl, z_rl - z_min);
    update_wheel_label(lbl_wheel_rr, z_rr - z_min);
}

