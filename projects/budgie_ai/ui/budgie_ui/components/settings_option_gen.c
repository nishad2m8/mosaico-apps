/**
 * @file settings_option_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "settings_option_gen.h"
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

lv_obj_t * settings_option_create(lv_obj_t * parent, const char * text, lv_subject_t * subject, int32_t value)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_option;
    static lv_style_t style_option_checked;
    static lv_style_t style_option_pressed;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_option);
        lv_style_init(&style_option_checked);
        lv_style_init(&style_option_pressed);

        lv_style_set_radius(&style_option, 999);
        lv_style_set_pad_all(&style_option, 0);
        lv_style_set_bg_opa(&style_option, 255);
        lv_style_set_bg_color(&style_option, COLOR_TRACK);
        lv_style_set_border_width(&style_option, 0);
        lv_style_set_shadow_width(&style_option, 0);
        lv_style_set_outline_width(&style_option, 0);
        lv_style_set_text_font(&style_option, font_row_title);
        lv_style_set_text_color(&style_option, COLOR_MUTED);
        lv_style_set_bg_color(&style_option_checked, COLOR_BTN_START);
        lv_style_set_bg_grad_color(&style_option_checked, COLOR_BTN_END);
        lv_style_set_bg_grad_dir(&style_option_checked, LV_GRAD_DIR_HOR);
        lv_style_set_text_color(&style_option_checked, lv_color_hex(0xffffff));
        lv_style_set_shadow_width(&style_option_checked, 10);
        lv_style_set_shadow_opa(&style_option_checked, 70);
        lv_style_set_shadow_color(&style_option_checked, COLOR_BTN_START);
        lv_style_set_shadow_offset_y(&style_option_checked, 2);
        lv_style_set_bg_color(&style_option_pressed, COLOR_KEY_PRESSED);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        lv_obj_t * lv_button_0 = lv_button_create(parent);
        lv_obj_set_name_static(lv_button_0, "settings_option_#");
        lv_obj_set_flex_grow(lv_button_0, 1);
        lv_obj_set_height(lv_button_0, 40);
        lv_obj_set_flag(lv_button_0, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);

        lv_obj_add_style(lv_button_0, &style_option, 0);
        lv_obj_add_style(lv_button_0, &style_option_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(lv_button_0, &style_option_checked, LV_STATE_CHECKED);
        lv_obj_bind_state_if_eq(lv_button_0, subject, LV_STATE_CHECKED, value);
        lv_obj_add_subject_set_int_event(lv_button_0, subject, LV_EVENT_CLICKED, value);
        lv_obj_t * option_label = lv_label_create(lv_button_0);
        lv_obj_set_name(option_label, "option_label");
        lv_label_set_text(option_label, text);
        lv_obj_set_align(option_label, LV_ALIGN_CENTER);

        the_root = lv_button_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

