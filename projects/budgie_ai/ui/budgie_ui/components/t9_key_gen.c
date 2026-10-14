/**
 * @file t9_key_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "t9_key_gen.h"
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

lv_obj_t * t9_key_create(lv_obj_t * parent, const char * digit, const char * letters)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "t9_key_#");
        lv_obj_set_width(lv_button_0, 108);
        lv_obj_set_height(lv_button_0, 47);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);

        lv_obj_add_style(lv_button_0, &style_key, 0);
        lv_obj_add_style(lv_button_0, &style_key_pressed, LV_STATE_PRESSED);
        lv_obj_t * letters_label = lv_label_create(lv_button_0);
        lv_obj_set_name(letters_label, "letters_label");
        lv_label_set_text(letters_label, letters);
        lv_obj_set_align(letters_label, LV_ALIGN_CENTER);
        lv_obj_set_style_text_font(letters_label, font_bubble_title, 0);
        lv_obj_set_style_text_color(letters_label, COLOR_TEXT, 0);

        lv_obj_t * digit_label = lv_label_create(lv_button_0);
        lv_obj_set_name(digit_label, "digit_label");
        lv_label_set_text(digit_label, digit);
        lv_obj_set_align(digit_label, LV_ALIGN_TOP_RIGHT);
        lv_obj_set_x(digit_label, -8);
        lv_obj_set_y(digit_label, 3);
        lv_obj_set_style_text_font(digit_label, font_key_digit, 0);
        lv_obj_set_style_text_color(digit_label, COLOR_MUTED, 0);

        lv_obj_add_event_cb(lv_button_0, chat_t9_key_cb, LV_EVENT_CLICKED, (void *)digit);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

