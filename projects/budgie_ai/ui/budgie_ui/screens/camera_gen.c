/**
 * @file camera_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "camera_gen.h"
#include "../budgie_ui.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 *  STATIC VARIABLES
 **********************/

/***********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * camera_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_cam_round;
    static lv_style_t style_cam_round_pressed;
    static lv_style_t style_cam_round_disabled;
    static lv_style_t style_icon_btn;
    static lv_style_t style_icon_btn_pressed;
    static lv_style_t style_shutter;
    static lv_style_t style_shutter_pressed;
    static lv_style_t style_corner;
    static lv_style_t style_hint;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_cam_round);
        lv_style_init(&style_cam_round_pressed);
        lv_style_init(&style_cam_round_disabled);
        lv_style_init(&style_icon_btn);
        lv_style_init(&style_icon_btn_pressed);
        lv_style_init(&style_shutter);
        lv_style_init(&style_shutter_pressed);
        lv_style_init(&style_corner);
        lv_style_init(&style_hint);

        lv_style_set_radius(&style_cam_round, 999);
        lv_style_set_pad_all(&style_cam_round, 0);
        lv_style_set_bg_opa(&style_cam_round, 190);
        lv_style_set_bg_color(&style_cam_round, COLOR_CAM_OVERLAY);
        lv_style_set_border_width(&style_cam_round, 0);
        lv_style_set_shadow_width(&style_cam_round, 0);
        lv_style_set_outline_width(&style_cam_round, 0);
        lv_style_set_bg_opa(&style_cam_round_pressed, 255);
        lv_style_set_opa(&style_cam_round_disabled, 90);
        lv_style_set_radius(&style_icon_btn, 999);
        lv_style_set_pad_all(&style_icon_btn, 0);
        lv_style_set_bg_opa(&style_icon_btn, 0);
        lv_style_set_border_width(&style_icon_btn, 0);
        lv_style_set_shadow_width(&style_icon_btn, 0);
        lv_style_set_outline_width(&style_icon_btn, 0);
        lv_style_set_bg_opa(&style_icon_btn_pressed, 40);
        lv_style_set_bg_color(&style_icon_btn_pressed, lv_color_hex(0xffffff));
        lv_style_set_radius(&style_shutter, 999);
        lv_style_set_pad_all(&style_shutter, 0);
        lv_style_set_bg_opa(&style_shutter, 255);
        lv_style_set_bg_color(&style_shutter, lv_color_hex(0xffffff));
        lv_style_set_border_width(&style_shutter, 0);
        lv_style_set_shadow_width(&style_shutter, 0);
        lv_style_set_outline_width(&style_shutter, 4);
        lv_style_set_outline_pad(&style_shutter, 5);
        lv_style_set_outline_color(&style_shutter, lv_color_hex(0xffffff));
        lv_style_set_outline_opa(&style_shutter, 255);
        lv_style_set_bg_color(&style_shutter_pressed, lv_color_hex(0xdfe5f0));
        lv_style_set_bg_opa(&style_corner, 255);
        lv_style_set_bg_color(&style_corner, lv_color_hex(0xffffff));
        lv_style_set_border_width(&style_corner, 0);
        lv_style_set_radius(&style_corner, 2);
        lv_style_set_pad_all(&style_corner, 0);
        lv_style_set_text_font(&style_hint, font_body);
        lv_style_set_text_color(&style_hint, lv_color_hex(0xffffff));
        lv_style_set_text_opa(&style_hint, 210);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "camera_#");
        lv_obj_set_style_bg_color(lv_obj_0, COLOR_CAM_BG, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, 255, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_t * viewfinder = lv_obj_create(lv_obj_0);
        lv_obj_set_name(viewfinder, "viewfinder");
        lv_obj_set_width(viewfinder, lv_pct(100));
        lv_obj_set_height(viewfinder, lv_pct(100));
        lv_obj_set_flag(viewfinder, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(viewfinder, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(viewfinder, 0, 0);
        lv_obj_set_style_border_width(viewfinder, 0, 0);
        lv_obj_set_style_pad_all(viewfinder, 0, 0);
        lv_obj_set_style_radius(viewfinder, 0, 0);

        lv_obj_t * frame = lv_obj_create(lv_obj_0);
        lv_obj_set_name(frame, "frame");
        lv_obj_set_width(frame, 216);
        lv_obj_set_height(frame, 216);
        lv_obj_set_align(frame, LV_ALIGN_CENTER);
        lv_obj_set_y(frame, -36);
        lv_obj_set_flag(frame, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(frame, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(frame, 0, 0);
        lv_obj_set_style_border_width(frame, 0, 0);
        lv_obj_set_style_pad_all(frame, 0, 0);
        lv_obj_bind_flag_if_eq(frame, &subject_photo_taken, LV_OBJ_FLAG_HIDDEN, 1);
        lv_obj_t * lv_obj_1 = lv_obj_create(frame);
        lv_obj_set_width(lv_obj_1, 32);
        lv_obj_set_height(lv_obj_1, 4);
        lv_obj_set_align(lv_obj_1, LV_ALIGN_TOP_LEFT);
        lv_obj_add_style(lv_obj_1, &style_corner, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(frame);
        lv_obj_set_width(lv_obj_2, 4);
        lv_obj_set_height(lv_obj_2, 32);
        lv_obj_set_align(lv_obj_2, LV_ALIGN_TOP_LEFT);
        lv_obj_add_style(lv_obj_2, &style_corner, 0);

        lv_obj_t * lv_obj_3 = lv_obj_create(frame);
        lv_obj_set_width(lv_obj_3, 32);
        lv_obj_set_height(lv_obj_3, 4);
        lv_obj_set_align(lv_obj_3, LV_ALIGN_TOP_RIGHT);
        lv_obj_add_style(lv_obj_3, &style_corner, 0);

        lv_obj_t * lv_obj_4 = lv_obj_create(frame);
        lv_obj_set_width(lv_obj_4, 4);
        lv_obj_set_height(lv_obj_4, 32);
        lv_obj_set_align(lv_obj_4, LV_ALIGN_TOP_RIGHT);
        lv_obj_add_style(lv_obj_4, &style_corner, 0);

        lv_obj_t * lv_obj_5 = lv_obj_create(frame);
        lv_obj_set_width(lv_obj_5, 32);
        lv_obj_set_height(lv_obj_5, 4);
        lv_obj_set_align(lv_obj_5, LV_ALIGN_BOTTOM_LEFT);
        lv_obj_add_style(lv_obj_5, &style_corner, 0);

        lv_obj_t * lv_obj_6 = lv_obj_create(frame);
        lv_obj_set_width(lv_obj_6, 4);
        lv_obj_set_height(lv_obj_6, 32);
        lv_obj_set_align(lv_obj_6, LV_ALIGN_BOTTOM_LEFT);
        lv_obj_add_style(lv_obj_6, &style_corner, 0);

        lv_obj_t * lv_obj_7 = lv_obj_create(frame);
        lv_obj_set_width(lv_obj_7, 32);
        lv_obj_set_height(lv_obj_7, 4);
        lv_obj_set_align(lv_obj_7, LV_ALIGN_BOTTOM_RIGHT);
        lv_obj_add_style(lv_obj_7, &style_corner, 0);

        lv_obj_t * lv_obj_8 = lv_obj_create(frame);
        lv_obj_set_width(lv_obj_8, 4);
        lv_obj_set_height(lv_obj_8, 32);
        lv_obj_set_align(lv_obj_8, LV_ALIGN_BOTTOM_RIGHT);
        lv_obj_add_style(lv_obj_8, &style_corner, 0);

        lv_obj_t * captured_chip = lv_obj_create(lv_obj_0);
        lv_obj_set_name(captured_chip, "captured_chip");
        lv_obj_set_width(captured_chip, LV_SIZE_CONTENT);
        lv_obj_set_height(captured_chip, LV_SIZE_CONTENT);
        lv_obj_set_align(captured_chip, LV_ALIGN_TOP_MID);
        lv_obj_set_y(captured_chip, 74);
        lv_obj_set_flag(captured_chip, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(captured_chip, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(captured_chip, 220, 0);
        lv_obj_set_style_bg_color(captured_chip, COLOR_CAM_OVERLAY, 0);
        lv_obj_set_style_border_width(captured_chip, 0, 0);
        lv_obj_set_style_radius(captured_chip, 999, 0);
        lv_obj_set_style_pad_hor(captured_chip, 14, 0);
        lv_obj_set_style_pad_ver(captured_chip, 6, 0);
        lv_obj_set_style_pad_column(captured_chip, 8, 0);
        lv_obj_set_style_layout(captured_chip, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(captured_chip, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_style_flex_cross_place(captured_chip, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_bind_flag_if_eq(captured_chip, &subject_photo_taken, LV_OBJ_FLAG_HIDDEN, 0);
        lv_obj_t * lv_obj_9 = lv_obj_create(captured_chip);
        lv_obj_set_width(lv_obj_9, 10);
        lv_obj_set_height(lv_obj_9, 10);
        lv_obj_set_style_bg_opa(lv_obj_9, 255, 0);
        lv_obj_set_style_bg_color(lv_obj_9, COLOR_BTN_END, 0);
        lv_obj_set_style_border_width(lv_obj_9, 0, 0);
        lv_obj_set_style_radius(lv_obj_9, 999, 0);

        lv_obj_t * lv_label_0 = lv_label_create(captured_chip);
        lv_label_set_text(lv_label_0, "Photo captured");
        lv_obj_set_style_text_font(lv_label_0, font_body, 0);
        lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0xffffff), 0);

        lv_obj_t * back_btn = lv_button_create(lv_obj_0);
        lv_obj_set_name(back_btn, "back_btn");
        lv_obj_set_width(back_btn, 44);
        lv_obj_set_height(back_btn, 44);
        lv_obj_set_x(back_btn, 8);
        lv_obj_set_y(back_btn, 6);
        lv_obj_set_ext_click_area(back_btn, 6);
        lv_obj_add_style(back_btn, &style_icon_btn, 0);
        lv_obj_add_style(back_btn, &style_icon_btn_pressed, LV_STATE_PRESSED);
        lv_obj_bind_flag_if_not_eq(back_btn, &subject_camera_origin, LV_OBJ_FLAG_HIDDEN, ORIGIN_HOME);
        lv_obj_t * lv_image_0 = lv_image_create(back_btn);
        lv_image_set_src(lv_image_0, img_icon_back_light);
        lv_obj_set_align(lv_image_0, LV_ALIGN_CENTER);

        lv_obj_add_subject_set_int_event(back_btn, &subject_photo_taken, LV_EVENT_CLICKED, 0);
        lv_obj_add_screen_load_event(back_btn, LV_EVENT_CLICKED, home, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 250, 0);

        lv_obj_t * back_btn_chat = lv_button_create(lv_obj_0);
        lv_obj_set_name(back_btn_chat, "back_btn_chat");
        lv_obj_set_width(back_btn_chat, 44);
        lv_obj_set_height(back_btn_chat, 44);
        lv_obj_set_x(back_btn_chat, 8);
        lv_obj_set_y(back_btn_chat, 6);
        lv_obj_set_ext_click_area(back_btn_chat, 6);
        lv_obj_add_style(back_btn_chat, &style_icon_btn, 0);
        lv_obj_add_style(back_btn_chat, &style_icon_btn_pressed, LV_STATE_PRESSED);
        lv_obj_bind_flag_if_not_eq(back_btn_chat, &subject_camera_origin, LV_OBJ_FLAG_HIDDEN, ORIGIN_CHAT);
        lv_obj_t * lv_image_1 = lv_image_create(back_btn_chat);
        lv_image_set_src(lv_image_1, img_icon_back_light);
        lv_obj_set_align(lv_image_1, LV_ALIGN_CENTER);

        lv_obj_add_subject_set_int_event(back_btn_chat, &subject_photo_taken, LV_EVENT_CLICKED, 0);
        lv_obj_add_screen_load_event(back_btn_chat, LV_EVENT_CLICKED, chat, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_1, "Camera");
        lv_obj_set_align(lv_label_1, LV_ALIGN_TOP_MID);
        lv_obj_set_y(lv_label_1, 16);
        lv_obj_set_style_text_font(lv_label_1, font_status, 0);
        lv_obj_set_style_text_color(lv_label_1, lv_color_hex(0xffffff), 0);

        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_2, "Point at something and tap the shutter");
        lv_obj_set_align(lv_label_2, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(lv_label_2, -124);
        lv_obj_add_style(lv_label_2, &style_hint, 0);
        lv_obj_bind_flag_if_eq(lv_label_2, &subject_photo_taken, LV_OBJ_FLAG_HIDDEN, 1);

        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_3, "Looks good? Tap send, or x to delete");
        lv_obj_set_align(lv_label_3, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(lv_label_3, -124);
        lv_obj_add_style(lv_label_3, &style_hint, 0);
        lv_obj_bind_flag_if_eq(lv_label_3, &subject_photo_taken, LV_OBJ_FLAG_HIDDEN, 0);

        lv_obj_t * delete_btn = lv_button_create(lv_obj_0);
        lv_obj_set_name(delete_btn, "delete_btn");
        lv_obj_set_width(delete_btn, 60);
        lv_obj_set_height(delete_btn, 60);
        lv_obj_set_align(delete_btn, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_x(delete_btn, -132);
        lv_obj_set_y(delete_btn, -42);
        lv_obj_add_style(delete_btn, &style_cam_round, 0);
        lv_obj_add_style(delete_btn, &style_cam_round_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(delete_btn, &style_cam_round_disabled, LV_STATE_DISABLED);
        lv_obj_bind_state_if_eq(delete_btn, &subject_photo_taken, LV_STATE_DISABLED, 0);
        lv_obj_t * lv_image_2 = lv_image_create(delete_btn);
        lv_image_set_src(lv_image_2, img_icon_close);
        lv_obj_set_align(lv_image_2, LV_ALIGN_CENTER);

        lv_obj_add_subject_set_int_event(delete_btn, &subject_photo_taken, LV_EVENT_CLICKED, 0);

        lv_obj_t * shutter_btn = lv_button_create(lv_obj_0);
        lv_obj_set_name(shutter_btn, "shutter_btn");
        lv_obj_set_width(shutter_btn, 68);
        lv_obj_set_height(shutter_btn, 68);
        lv_obj_set_align(shutter_btn, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(shutter_btn, -38);
        lv_obj_add_style(shutter_btn, &style_shutter, 0);
        lv_obj_add_style(shutter_btn, &style_shutter_pressed, LV_STATE_PRESSED);
        lv_obj_add_subject_set_int_event(shutter_btn, &subject_photo_taken, LV_EVENT_CLICKED, 1);

        lv_obj_t * send_btn = lv_button_create(lv_obj_0);
        lv_obj_set_name(send_btn, "send_btn");
        lv_obj_set_width(send_btn, 60);
        lv_obj_set_height(send_btn, 60);
        lv_obj_set_align(send_btn, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_x(send_btn, 132);
        lv_obj_set_y(send_btn, -42);
        lv_obj_add_style(send_btn, &style_round_btn, 0);
        lv_obj_add_style(send_btn, &style_round_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(send_btn, &style_round_btn_disabled, LV_STATE_DISABLED);
        lv_obj_bind_flag_if_not_eq(send_btn, &subject_camera_origin, LV_OBJ_FLAG_HIDDEN, ORIGIN_HOME);
        lv_obj_bind_state_if_eq(send_btn, &subject_photo_taken, LV_STATE_DISABLED, 0);
        lv_obj_t * lv_image_3 = lv_image_create(send_btn);
        lv_image_set_src(lv_image_3, img_icon_send_large);
        lv_obj_set_align(lv_image_3, LV_ALIGN_CENTER);

        lv_obj_add_subject_set_int_event(send_btn, &subject_photo_taken, LV_EVENT_CLICKED, 0);
        lv_obj_add_subject_set_int_event(send_btn, &subject_budgie_mode, LV_EVENT_CLICKED, MODE_THINKING);
        lv_obj_add_screen_load_event(send_btn, LV_EVENT_CLICKED, home, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0);

        lv_obj_t * send_btn_chat = lv_button_create(lv_obj_0);
        lv_obj_set_name(send_btn_chat, "send_btn_chat");
        lv_obj_set_width(send_btn_chat, 60);
        lv_obj_set_height(send_btn_chat, 60);
        lv_obj_set_align(send_btn_chat, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_x(send_btn_chat, 132);
        lv_obj_set_y(send_btn_chat, -42);
        lv_obj_add_style(send_btn_chat, &style_round_btn, 0);
        lv_obj_add_style(send_btn_chat, &style_round_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(send_btn_chat, &style_round_btn_disabled, LV_STATE_DISABLED);
        lv_obj_bind_flag_if_not_eq(send_btn_chat, &subject_camera_origin, LV_OBJ_FLAG_HIDDEN, ORIGIN_CHAT);
        lv_obj_bind_state_if_eq(send_btn_chat, &subject_photo_taken, LV_STATE_DISABLED, 0);
        lv_obj_t * lv_image_4 = lv_image_create(send_btn_chat);
        lv_image_set_src(lv_image_4, img_icon_send_large);
        lv_obj_set_align(lv_image_4, LV_ALIGN_CENTER);

        lv_obj_add_subject_set_int_event(send_btn_chat, &subject_chat_has_photo, LV_EVENT_CLICKED, 1);
        lv_obj_add_subject_set_int_event(send_btn_chat, &subject_photo_taken, LV_EVENT_CLICKED, 0);
        lv_obj_add_screen_load_event(send_btn_chat, LV_EVENT_CLICKED, chat, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

