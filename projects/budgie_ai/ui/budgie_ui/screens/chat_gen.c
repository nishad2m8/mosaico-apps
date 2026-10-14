/**
 * @file chat_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "chat_gen.h"
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

lv_obj_t * chat_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static bool screen_created = false;
    if (screen_created) {
        LV_LOG_WARN("`chat` is already initialized as a permanent screen. Returning with no change.");
        return chat;
    }

    static lv_style_t style_icon_btn;
    static lv_style_t style_icon_btn_pressed;
    static lv_style_t style_card;
    static lv_style_t style_key_text;
    static lv_style_t style_placeholder;
    static lv_style_t style_bar_cursor;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_icon_btn);
        lv_style_init(&style_icon_btn_pressed);
        lv_style_init(&style_card);
        lv_style_init(&style_key_text);
        lv_style_init(&style_placeholder);
        lv_style_init(&style_bar_cursor);

        lv_style_set_radius(&style_icon_btn, 999);
        lv_style_set_pad_all(&style_icon_btn, 0);
        lv_style_set_bg_opa(&style_icon_btn, 0);
        lv_style_set_border_width(&style_icon_btn, 0);
        lv_style_set_shadow_width(&style_icon_btn, 0);
        lv_style_set_outline_width(&style_icon_btn, 0);
        lv_style_set_bg_opa(&style_icon_btn_pressed, 255);
        lv_style_set_bg_color(&style_icon_btn_pressed, COLOR_KEY_PRESSED);
        lv_style_set_radius(&style_card, 20);
        lv_style_set_bg_opa(&style_card, 255);
        lv_style_set_bg_color(&style_card, COLOR_CARD);
        lv_style_set_border_width(&style_card, 1);
        lv_style_set_border_color(&style_card, COLOR_BUBBLE_BORDER);
        lv_style_set_text_font(&style_key_text, font_bubble_title);
        lv_style_set_text_color(&style_key_text, COLOR_TEXT);
        lv_style_set_text_color(&style_placeholder, COLOR_MUTED);
        lv_style_set_bg_opa(&style_bar_cursor, 0);
        lv_style_set_border_side(&style_bar_cursor, LV_BORDER_SIDE_LEFT);
        lv_style_set_border_width(&style_bar_cursor, 2);
        lv_style_set_border_color(&style_bar_cursor, COLOR_BTN_START);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        chat = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = chat;
        lv_obj_set_name_static(lv_obj_0, "chat_#");
        lv_obj_set_style_bg_color(lv_obj_0, COLOR_BG, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, 255, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_event_cb(lv_obj_0, chat_screen_load_cb, LV_EVENT_SCREEN_LOAD_START, NULL);
        lv_obj_t * back_btn = lv_button_create(lv_obj_0);
        lv_obj_set_name(back_btn, "back_btn");
        lv_obj_set_width(back_btn, 44);
        lv_obj_set_height(back_btn, 44);
        lv_obj_set_x(back_btn, 8);
        lv_obj_set_y(back_btn, 6);
        lv_obj_set_ext_click_area(back_btn, 6);
        lv_obj_add_style(back_btn, &style_icon_btn, 0);
        lv_obj_add_style(back_btn, &style_icon_btn_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_0 = lv_image_create(back_btn);
        lv_image_set_src(lv_image_0, img_icon_back);
        lv_obj_set_align(lv_image_0, LV_ALIGN_CENTER);

        lv_obj_add_screen_load_event(back_btn, LV_EVENT_CLICKED, home, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 250, 0);

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_0, "Chat with Budgie");
        lv_obj_set_align(lv_label_0, LV_ALIGN_TOP_MID);
        lv_obj_set_y(lv_label_0, 16);
        lv_obj_set_style_text_font(lv_label_0, font_status, 0);
        lv_obj_set_style_text_color(lv_label_0, COLOR_TEXT, 0);

        lv_obj_t * response_card = lv_obj_create(lv_obj_0);
        lv_obj_set_name(response_card, "response_card");
        lv_obj_set_x(response_card, 16);
        lv_obj_set_y(response_card, 60);
        lv_obj_set_width(response_card, 448);
        lv_obj_set_height(response_card, 132);
        lv_obj_set_flag(response_card, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_pad_all(response_card, 12, 0);
        lv_obj_set_style_pad_column(response_card, 12, 0);
        lv_obj_set_style_layout(response_card, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(response_card, LV_FLEX_FLOW_ROW, 0);
        lv_obj_add_style(response_card, &style_card, 0);
        lv_obj_t * lv_image_1 = lv_image_create(response_card);
        lv_image_set_src(lv_image_1, img_budgie_avatar);

        lv_obj_t * response_scroll = lv_obj_create(response_card);
        lv_obj_set_name(response_scroll, "response_scroll");
        lv_obj_set_flex_grow(response_scroll, 1);
        lv_obj_set_height(response_scroll, lv_pct(100));
        lv_obj_set_scrollbar_mode(response_scroll, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_bg_opa(response_scroll, 0, 0);
        lv_obj_set_style_border_width(response_scroll, 0, 0);
        lv_obj_set_style_pad_all(response_scroll, 0, 0);
        lv_obj_set_style_pad_top(response_scroll, 4, 0);
        lv_obj_t * response = lv_label_create(response_scroll);
        lv_obj_set_name(response, "response");
        lv_obj_set_width(response, lv_pct(100));
        lv_label_set_long_mode(response, LV_LABEL_LONG_MODE_WRAP);
        lv_label_set_text(response, "Hi! Type your message on the keypad and tap Send. I'll answer here.");
        lv_obj_set_style_text_font(response, font_bubble_text, 0);
        lv_obj_set_style_text_color(response, COLOR_TEXT, 0);
        lv_obj_set_style_text_line_space(response, 4, 0);

        lv_obj_t * chat_input = lv_textarea_create(lv_obj_0);
        lv_obj_set_name(chat_input, "chat_input");
        lv_obj_set_x(chat_input, 16);
        lv_obj_set_y(chat_input, 200);
        lv_obj_set_width(chat_input, 392);
        lv_obj_set_height(chat_input, 48);
        lv_textarea_set_one_line(chat_input, true);
        lv_obj_set_state(chat_input, LV_STATE_FOCUSED, true);
        lv_textarea_set_placeholder_text(chat_input, "Type a message...");
        lv_obj_set_style_radius(chat_input, 24, 0);
        lv_obj_set_style_bg_opa(chat_input, 255, 0);
        lv_obj_set_style_bg_color(chat_input, COLOR_CARD, 0);
        lv_obj_set_style_border_width(chat_input, 1, 0);
        lv_obj_set_style_border_color(chat_input, COLOR_BUBBLE_BORDER, 0);
        lv_obj_set_style_pad_hor(chat_input, 18, 0);
        lv_obj_set_style_pad_ver(chat_input, 12, 0);
        lv_obj_set_style_text_font(chat_input, font_bubble_text, 0);
        lv_obj_set_style_text_color(chat_input, COLOR_TEXT, 0);
        lv_obj_add_style(chat_input, &style_placeholder, LV_PART_TEXTAREA_PLACEHOLDER);
        lv_obj_add_style(chat_input, &style_bar_cursor, LV_PART_CURSOR | LV_STATE_FOCUSED);

        lv_obj_t * attach_btn = lv_button_create(lv_obj_0);
        lv_obj_set_name(attach_btn, "attach_btn");
        lv_obj_set_x(attach_btn, 416);
        lv_obj_set_y(attach_btn, 200);
        lv_obj_set_width(attach_btn, 48);
        lv_obj_set_height(attach_btn, 48);
        lv_obj_set_flag(attach_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_set_style_outline_width(attach_btn, 0, 0);
        lv_obj_set_style_shadow_width(attach_btn, 12, 0);
        lv_obj_set_style_shadow_offset_y(attach_btn, 3, 0);
        lv_obj_add_style(attach_btn, &style_round_btn, 0);
        lv_obj_add_style(attach_btn, &style_round_btn_pressed, LV_STATE_PRESSED);
        lv_obj_bind_flag_if_eq(attach_btn, &subject_chat_has_photo, LV_OBJ_FLAG_HIDDEN, 1);
        lv_obj_t * lv_image_2 = lv_image_create(attach_btn);
        lv_image_set_src(lv_image_2, img_icon_camera_small);
        lv_obj_set_align(lv_image_2, LV_ALIGN_CENTER);

        lv_obj_add_subject_set_int_event(attach_btn, &subject_photo_taken, LV_EVENT_CLICKED, 0);
        lv_obj_add_subject_set_int_event(attach_btn, &subject_camera_origin, LV_EVENT_CLICKED, ORIGIN_CHAT);
        lv_obj_add_screen_create_event(attach_btn, LV_EVENT_CLICKED, camera_create, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0);

        lv_obj_t * photo_tile = lv_obj_create(lv_obj_0);
        lv_obj_set_name(photo_tile, "photo_tile");
        lv_obj_set_x(photo_tile, 416);
        lv_obj_set_y(photo_tile, 200);
        lv_obj_set_width(photo_tile, 48);
        lv_obj_set_height(photo_tile, 48);
        lv_obj_set_flag(photo_tile, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(photo_tile, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_radius(photo_tile, 14, 0);
        lv_obj_set_style_border_width(photo_tile, 0, 0);
        lv_obj_set_style_pad_all(photo_tile, 0, 0);
        lv_obj_set_style_bg_opa(photo_tile, 255, 0);
        lv_obj_set_style_bg_color(photo_tile, COLOR_BTN_START, 0);
        lv_obj_set_style_bg_grad_color(photo_tile, COLOR_BTN_END, 0);
        lv_obj_set_style_bg_grad_dir(photo_tile, LV_GRAD_DIR_HOR, 0);
        lv_obj_bind_flag_if_eq(photo_tile, &subject_chat_has_photo, LV_OBJ_FLAG_HIDDEN, 0);
        lv_obj_t * photo_thumb = lv_image_create(photo_tile);
        lv_obj_set_name(photo_thumb, "photo_thumb");
        lv_image_set_src(photo_thumb, img_icon_photo);
        lv_obj_set_align(photo_thumb, LV_ALIGN_CENTER);

        lv_obj_t * photo_remove_btn = lv_button_create(lv_obj_0);
        lv_obj_set_name(photo_remove_btn, "photo_remove_btn");
        lv_obj_set_x(photo_remove_btn, 450);
        lv_obj_set_y(photo_remove_btn, 192);
        lv_obj_set_width(photo_remove_btn, 22);
        lv_obj_set_height(photo_remove_btn, 22);
        lv_obj_set_ext_click_area(photo_remove_btn, 8);
        lv_obj_set_flag(photo_remove_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_set_style_radius(photo_remove_btn, 999, 0);
        lv_obj_set_style_pad_all(photo_remove_btn, 0, 0);
        lv_obj_set_style_bg_opa(photo_remove_btn, 255, 0);
        lv_obj_set_style_bg_color(photo_remove_btn, COLOR_TEXT, 0);
        lv_obj_set_style_border_width(photo_remove_btn, 2, 0);
        lv_obj_set_style_border_color(photo_remove_btn, COLOR_BG, 0);
        lv_obj_set_style_shadow_width(photo_remove_btn, 0, 0);
        lv_obj_set_style_outline_width(photo_remove_btn, 0, 0);
        lv_obj_bind_flag_if_eq(photo_remove_btn, &subject_chat_has_photo, LV_OBJ_FLAG_HIDDEN, 0);
        lv_obj_t * lv_image_3 = lv_image_create(photo_remove_btn);
        lv_image_set_src(lv_image_3, img_icon_close_small);
        lv_obj_set_align(lv_image_3, LV_ALIGN_CENTER);

        lv_obj_add_subject_set_int_event(photo_remove_btn, &subject_chat_has_photo, LV_EVENT_CLICKED, 0);

        lv_obj_t * keypad = lv_obj_create(lv_obj_0);
        lv_obj_set_name(keypad, "keypad");
        lv_obj_set_x(keypad, 12);
        lv_obj_set_y(keypad, 256);
        lv_obj_set_width(keypad, 456);
        lv_obj_set_height(keypad, 212);
        lv_obj_set_flag(keypad, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(keypad, 0, 0);
        lv_obj_set_style_border_width(keypad, 0, 0);
        lv_obj_set_style_pad_all(keypad, 0, 0);
        lv_obj_set_style_pad_row(keypad, 8, 0);
        lv_obj_set_style_pad_column(keypad, 8, 0);
        lv_obj_set_style_layout(keypad, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(keypad, LV_FLEX_FLOW_ROW_WRAP, 0);
        t9_key_create(keypad, "1", ".,?!");

        t9_key_create(keypad, "2", "abc");

        t9_key_create(keypad, "3", "def");

        lv_obj_t * backspace_key = lv_button_create(keypad);
        lv_obj_set_name(backspace_key, "backspace_key");
        lv_obj_set_flag(backspace_key, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_set_width(backspace_key, 108);
        lv_obj_set_height(backspace_key, 47);
        lv_obj_add_style(backspace_key, &style_key, 0);
        lv_obj_add_style(backspace_key, &style_key_fn, 0);
        lv_obj_add_style(backspace_key, &style_key_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_4 = lv_image_create(backspace_key);
        lv_image_set_src(lv_image_4, img_icon_backspace);
        lv_obj_set_align(lv_image_4, LV_ALIGN_CENTER);

        lv_obj_add_event_cb(backspace_key, chat_backspace_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(backspace_key, chat_backspace_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);

        t9_key_create(keypad, "4", "ghi");

        t9_key_create(keypad, "5", "jkl");

        t9_key_create(keypad, "6", "mno");

        lv_obj_t * case_key = lv_button_create(keypad);
        lv_obj_set_name(case_key, "case_key");
        lv_obj_set_flag(case_key, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_set_width(case_key, 108);
        lv_obj_set_height(case_key, 47);
        lv_obj_add_style(case_key, &style_key, 0);
        lv_obj_add_style(case_key, &style_key_fn, 0);
        lv_obj_add_style(case_key, &style_key_pressed, LV_STATE_PRESSED);
        lv_obj_t * case_label = lv_label_create(case_key);
        lv_obj_set_name(case_label, "case_label");
        lv_label_set_text(case_label, "abc");
        lv_obj_set_align(case_label, LV_ALIGN_CENTER);
        lv_obj_add_style(case_label, &style_key_text, 0);

        lv_obj_add_event_cb(case_key, chat_case_cb, LV_EVENT_CLICKED, NULL);

        t9_key_create(keypad, "7", "pqrs");

        t9_key_create(keypad, "8", "tuv");

        t9_key_create(keypad, "9", "wxyz");

        lv_obj_t * mode_key = lv_button_create(keypad);
        lv_obj_set_name(mode_key, "mode_key");
        lv_obj_set_flag(mode_key, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_set_width(mode_key, 108);
        lv_obj_set_height(mode_key, 47);
        lv_obj_add_style(mode_key, &style_key, 0);
        lv_obj_add_style(mode_key, &style_key_fn, 0);
        lv_obj_add_style(mode_key, &style_key_pressed, LV_STATE_PRESSED);
        lv_obj_t * mode_label = lv_label_create(mode_key);
        lv_obj_set_name(mode_label, "mode_label");
        lv_label_set_text(mode_label, "123");
        lv_obj_set_align(mode_label, LV_ALIGN_CENTER);
        lv_obj_add_style(mode_label, &style_key_text, 0);

        lv_obj_add_event_cb(mode_key, chat_mode_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * space_key = lv_button_create(keypad);
        lv_obj_set_name(space_key, "space_key");
        lv_obj_set_flag(space_key, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_set_width(space_key, 224);
        lv_obj_set_height(space_key, 47);
        lv_obj_add_style(space_key, &style_key, 0);
        lv_obj_add_style(space_key, &style_key_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_label_1 = lv_label_create(space_key);
        lv_label_set_text(lv_label_1, "space");
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
        lv_obj_add_style(lv_label_1, &style_key_text, 0);

        lv_obj_t * lv_label_2 = lv_label_create(space_key);
        lv_label_set_text(lv_label_2, "0");
        lv_obj_set_align(lv_label_2, LV_ALIGN_TOP_RIGHT);
        lv_obj_set_x(lv_label_2, -8);
        lv_obj_set_y(lv_label_2, 3);
        lv_obj_set_style_text_font(lv_label_2, font_key_digit, 0);
        lv_obj_set_style_text_color(lv_label_2, COLOR_MUTED, 0);

        lv_obj_add_event_cb(space_key, chat_space_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * clear_key = lv_button_create(keypad);
        lv_obj_set_name(clear_key, "clear_key");
        lv_obj_set_flag(clear_key, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_set_width(clear_key, 108);
        lv_obj_set_height(clear_key, 47);
        lv_obj_add_style(clear_key, &style_key, 0);
        lv_obj_add_style(clear_key, &style_key_fn, 0);
        lv_obj_add_style(clear_key, &style_key_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_label_3 = lv_label_create(clear_key);
        lv_label_set_text(lv_label_3, "Clear");
        lv_obj_set_align(lv_label_3, LV_ALIGN_CENTER);
        lv_obj_add_style(lv_label_3, &style_key_text, 0);

        lv_obj_add_event_cb(clear_key, chat_clear_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * send_key = lv_button_create(keypad);
        lv_obj_set_name(send_key, "send_key");
        lv_obj_set_flag(send_key, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_set_width(send_key, 108);
        lv_obj_set_height(send_key, 47);
        lv_obj_set_style_radius(send_key, 14, 0);
        lv_obj_set_style_outline_width(send_key, 0, 0);
        lv_obj_set_style_shadow_width(send_key, 12, 0);
        lv_obj_set_style_shadow_offset_y(send_key, 3, 0);
        lv_obj_add_style(send_key, &style_round_btn, 0);
        lv_obj_add_style(send_key, &style_round_btn_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_obj_1 = lv_obj_create(send_key);
        lv_obj_set_width(lv_obj_1, LV_SIZE_CONTENT);
        lv_obj_set_height(lv_obj_1, LV_SIZE_CONTENT);
        lv_obj_set_align(lv_obj_1, LV_ALIGN_CENTER);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_1, 0, 0);
        lv_obj_set_style_border_width(lv_obj_1, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_1, 0, 0);
        lv_obj_set_style_pad_column(lv_obj_1, 6, 0);
        lv_obj_set_style_layout(lv_obj_1, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(lv_obj_1, LV_FLEX_FLOW_ROW, 0);
        lv_obj_set_style_flex_cross_place(lv_obj_1, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_t * lv_label_4 = lv_label_create(lv_obj_1);
        lv_label_set_text(lv_label_4, "Send");
        lv_obj_set_style_text_font(lv_label_4, font_bubble_title, 0);
        lv_obj_set_style_text_color(lv_label_4, lv_color_hex(0xffffff), 0);

        lv_obj_t * lv_image_5 = lv_image_create(lv_obj_1);
        lv_image_set_src(lv_image_5, img_icon_send);

        lv_obj_add_event_cb(send_key, chat_send_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_subject_set_int_event(send_key, &subject_chat_has_photo, LV_EVENT_CLICKED, 0);

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

