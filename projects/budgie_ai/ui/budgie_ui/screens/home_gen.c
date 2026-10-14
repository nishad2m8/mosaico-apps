/**
 * @file home_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "home_gen.h"
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

lv_obj_t * home_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static bool screen_created = false;
    if (screen_created) {
        LV_LOG_WARN("`home` is already initialized as a permanent screen. Returning with no change.");
        return home;
    }

    static lv_style_t style_mic_live;
    static lv_style_t style_clear_btn;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_mic_live);
        lv_style_init(&style_clear_btn);

        lv_style_set_outline_width(&style_mic_live, 14);
        lv_style_set_outline_color(&style_mic_live, COLOR_BTN_END);
        lv_style_set_outline_opa(&style_mic_live, 110);
        lv_style_set_bg_color(&style_clear_btn, COLOR_DANGER);
        lv_style_set_bg_grad_color(&style_clear_btn, COLOR_DANGER);
        lv_style_set_outline_width(&style_clear_btn, 4);
        lv_style_set_outline_color(&style_clear_btn, COLOR_DANGER_HALO);
        lv_style_set_shadow_width(&style_clear_btn, 12);
        lv_style_set_shadow_offset_y(&style_clear_btn, 3);
        lv_style_set_shadow_color(&style_clear_btn, COLOR_DANGER);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        home = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = home;
        lv_obj_set_name_static(lv_obj_0, "home_#");
        lv_obj_set_style_bg_color(lv_obj_0, COLOR_BG, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, 255, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, "Budgie");
        lv_obj_set_align(status_bar_0, LV_ALIGN_TOP_MID);

        lv_obj_t * subtitle_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(subtitle_label, "subtitle_label");
        lv_label_set_text(subtitle_label, "Personal AI Assistant");
        lv_obj_set_align(subtitle_label, LV_ALIGN_TOP_MID);
        lv_obj_set_y(subtitle_label, 62);
        lv_obj_set_style_text_font(subtitle_label, font_body, 0);
        lv_obj_set_style_text_color(subtitle_label, COLOR_MUTED, 0);

        lv_obj_t * budgie_idle = lv_image_create(lv_obj_0);
        lv_obj_set_name(budgie_idle, "budgie_idle");
        lv_image_set_src(budgie_idle, img_budgie_idle);
        lv_obj_set_align(budgie_idle, LV_ALIGN_TOP_MID);
        lv_obj_set_x(budgie_idle, -20);
        lv_obj_set_y(budgie_idle, 104);
        lv_obj_bind_flag_if_not_eq(budgie_idle, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_IDLE);

        lv_obj_t * budgie_listening = lv_image_create(lv_obj_0);
        lv_obj_set_name(budgie_listening, "budgie_listening");
        lv_image_set_src(budgie_listening, img_budgie_listening);
        lv_obj_set_align(budgie_listening, LV_ALIGN_TOP_MID);
        lv_obj_set_x(budgie_listening, -20);
        lv_obj_set_y(budgie_listening, 104);
        lv_obj_bind_flag_if_not_eq(budgie_listening, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_LISTENING);

        lv_obj_t * budgie_thinking = lv_image_create(lv_obj_0);
        lv_obj_set_name(budgie_thinking, "budgie_thinking");
        lv_image_set_src(budgie_thinking, img_budgie_thinking);
        lv_obj_set_align(budgie_thinking, LV_ALIGN_TOP_MID);
        lv_obj_set_x(budgie_thinking, -20);
        lv_obj_set_y(budgie_thinking, 104);
        lv_obj_bind_flag_if_not_eq(budgie_thinking, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_THINKING);

        lv_obj_t * budgie_talking = lv_image_create(lv_obj_0);
        lv_obj_set_name(budgie_talking, "budgie_talking");
        lv_image_set_src(budgie_talking, img_budgie_talking);
        lv_obj_set_align(budgie_talking, LV_ALIGN_TOP_MID);
        lv_obj_set_x(budgie_talking, -20);
        lv_obj_set_y(budgie_talking, 104);
        lv_obj_bind_flag_if_not_eq(budgie_talking, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_TALKING);

        lv_obj_t * budgie_recorded = lv_image_create(lv_obj_0);
        lv_obj_set_name(budgie_recorded, "budgie_recorded");
        lv_image_set_src(budgie_recorded, img_budgie_idle);
        lv_obj_set_align(budgie_recorded, LV_ALIGN_TOP_MID);
        lv_obj_set_x(budgie_recorded, -20);
        lv_obj_set_y(budgie_recorded, 104);
        lv_obj_bind_flag_if_not_eq(budgie_recorded, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_RECORDED);

        lv_obj_t * bubble_idle = speech_bubble_create(lv_obj_0, "Hi! I'm Budgie.", "How can I help you today?");
        lv_obj_set_name(bubble_idle, "bubble_idle");
        lv_obj_set_align(bubble_idle, LV_ALIGN_TOP_MID);
        lv_obj_set_y(bubble_idle, 238);
        lv_obj_bind_flag_if_not_eq(bubble_idle, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_IDLE);

        lv_obj_t * bubble_listening = speech_bubble_create(lv_obj_0, "I'm listening...", "Go ahead, ask me anything.");
        lv_obj_set_name(bubble_listening, "bubble_listening");
        lv_obj_set_align(bubble_listening, LV_ALIGN_TOP_MID);
        lv_obj_set_y(bubble_listening, 238);
        lv_obj_bind_flag_if_not_eq(bubble_listening, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_LISTENING);

        lv_obj_t * bubble_thinking = speech_bubble_create(lv_obj_0, "Hmm, let me think...", "One moment please.");
        lv_obj_set_name(bubble_thinking, "bubble_thinking");
        lv_obj_set_align(bubble_thinking, LV_ALIGN_TOP_MID);
        lv_obj_set_y(bubble_thinking, 238);
        lv_obj_bind_flag_if_not_eq(bubble_thinking, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_THINKING);

        lv_obj_t * bubble_talking = speech_bubble_create(lv_obj_0, "Here's what I think!", "Tap the mic to ask more.");
        lv_obj_set_name(bubble_talking, "bubble_talking");
        lv_obj_set_align(bubble_talking, LV_ALIGN_TOP_MID);
        lv_obj_set_y(bubble_talking, 238);
        lv_obj_bind_flag_if_not_eq(bubble_talking, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_TALKING);

        lv_obj_t * bubble_recorded = speech_bubble_create(lv_obj_0, "Got it!", "Tap to send");
        lv_obj_set_name(bubble_recorded, "bubble_recorded");
        lv_obj_set_align(bubble_recorded, LV_ALIGN_TOP_MID);
        lv_obj_set_y(bubble_recorded, 238);
        lv_obj_bind_flag_if_not_eq(bubble_recorded, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_RECORDED);

        lv_obj_t * clear_btn = lv_button_create(lv_obj_0);
        lv_obj_set_name(clear_btn, "clear_btn");
        lv_obj_set_width(clear_btn, 40);
        lv_obj_set_height(clear_btn, 40);
        lv_obj_set_x(clear_btn, 142);
        lv_obj_set_y(clear_btn, 228);
        lv_obj_set_ext_click_area(clear_btn, 6);
        lv_obj_add_style(clear_btn, &style_round_btn, 0);
        lv_obj_add_style(clear_btn, &style_clear_btn, 0);
        lv_obj_add_style(clear_btn, &style_round_btn_pressed, LV_STATE_PRESSED);
        lv_obj_bind_flag_if_not_eq(clear_btn, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_RECORDED);
        lv_obj_t * lv_image_0 = lv_image_create(clear_btn);
        lv_image_set_src(lv_image_0, img_icon_close);
        lv_obj_set_align(lv_image_0, LV_ALIGN_CENTER);

        lv_obj_add_event_cb(clear_btn, home_clear_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * actions = lv_obj_create(lv_obj_0);
        lv_obj_set_name(actions, "actions");
        lv_obj_set_width(actions, LV_SIZE_CONTENT);
        lv_obj_set_height(actions, LV_SIZE_CONTENT);
        lv_obj_set_align(actions, LV_ALIGN_TOP_MID);
        lv_obj_set_y(actions, 336);
        lv_obj_set_flag(actions, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(actions, 0, 0);
        lv_obj_set_style_border_width(actions, 0, 0);
        lv_obj_set_style_pad_all(actions, 16, 0);
        lv_obj_set_style_layout(actions, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(actions, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_style_flex_cross_place(actions, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_column(actions, 40, 0);
        lv_obj_t * keyboard_btn = lv_button_create(actions);
        lv_obj_set_name(keyboard_btn, "keyboard_btn");
        lv_obj_set_width(keyboard_btn, 64);
        lv_obj_set_height(keyboard_btn, 64);
        lv_obj_add_style(keyboard_btn, &style_round_btn, 0);
        lv_obj_add_style(keyboard_btn, &style_round_btn_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_1 = lv_image_create(keyboard_btn);
        lv_image_set_src(lv_image_1, img_icon_keyboard);
        lv_obj_set_align(lv_image_1, LV_ALIGN_CENTER);

        lv_obj_add_screen_load_event(keyboard_btn, LV_EVENT_CLICKED, chat, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, 250, 0);

        lv_obj_t * mic_btn = lv_button_create(actions);
        lv_obj_set_name(mic_btn, "mic_btn");
        lv_obj_set_width(mic_btn, 84);
        lv_obj_set_height(mic_btn, 84);
        lv_obj_add_style(mic_btn, &style_round_btn, 0);
        lv_obj_add_style(mic_btn, &style_round_btn_pressed, LV_STATE_PRESSED);
        lv_obj_bind_style(mic_btn, &style_mic_live, 0, &subject_budgie_mode, MODE_LISTENING);
        lv_obj_t * mic_icon = lv_obj_create(mic_btn);
        lv_obj_set_name(mic_icon, "mic_icon");
        lv_obj_set_width(mic_icon, 40);
        lv_obj_set_height(mic_icon, 40);
        lv_obj_set_align(mic_icon, LV_ALIGN_CENTER);
        lv_obj_set_flag(mic_icon, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(mic_icon, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(mic_icon, 0, 0);
        lv_obj_set_style_border_width(mic_icon, 0, 0);
        lv_obj_set_style_pad_all(mic_icon, 0, 0);
        lv_obj_set_style_radius(mic_icon, 0, 0);
        lv_obj_bind_flag_if_eq(mic_icon, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_LISTENING);
        lv_obj_t * lv_image_2 = lv_image_create(mic_icon);
        lv_image_set_src(lv_image_2, img_icon_mic);
        lv_obj_set_align(lv_image_2, LV_ALIGN_CENTER);
        lv_obj_bind_flag_if_eq(lv_image_2, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_RECORDED);

        lv_obj_t * send = lv_image_create(mic_btn);
        lv_obj_set_name(send, "send");
        lv_image_set_src(send, img_icon_send_large);
        lv_obj_set_align(send, LV_ALIGN_CENTER);
        lv_obj_bind_flag_if_not_eq(send, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_RECORDED);

        lv_obj_t * stop = lv_obj_create(mic_btn);
        lv_obj_set_name(stop, "stop");
        lv_obj_set_width(stop, 24);
        lv_obj_set_height(stop, 24);
        lv_obj_set_align(stop, LV_ALIGN_CENTER);
        lv_obj_set_flag(stop, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(stop, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(stop, 255, 0);
        lv_obj_set_style_bg_color(stop, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_border_width(stop, 0, 0);
        lv_obj_set_style_radius(stop, 6, 0);
        lv_obj_bind_flag_if_not_eq(stop, &subject_budgie_mode, LV_OBJ_FLAG_HIDDEN, MODE_LISTENING);

        lv_obj_add_event_cb(mic_btn, home_mic_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * camera_btn = lv_button_create(actions);
        lv_obj_set_name(camera_btn, "camera_btn");
        lv_obj_set_width(camera_btn, 64);
        lv_obj_set_height(camera_btn, 64);
        lv_obj_add_style(camera_btn, &style_round_btn, 0);
        lv_obj_add_style(camera_btn, &style_round_btn_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_3 = lv_image_create(camera_btn);
        lv_image_set_src(lv_image_3, img_icon_camera);
        lv_obj_set_align(lv_image_3, LV_ALIGN_CENTER);

        lv_obj_add_subject_set_int_event(camera_btn, &subject_photo_taken, LV_EVENT_CLICKED, 0);
        lv_obj_add_subject_set_int_event(camera_btn, &subject_camera_origin, LV_EVENT_CLICKED, ORIGIN_HOME);
        lv_obj_add_screen_create_event(camera_btn, LV_EVENT_CLICKED, camera_create, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0);

        the_root = lv_obj_0;
    }
    #endif

    if (the_root) screen_created = true;

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

