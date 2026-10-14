/**
 * @file settings_choice_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "settings_choice_gen.h"
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

lv_obj_t * settings_choice_create(lv_obj_t * parent, const char * title, const char * subtitle)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "settings_choice_#");
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, LV_SIZE_CONTENT);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, 0, 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_hor(lv_obj_0, 16, 0);
        lv_obj_set_style_pad_ver(lv_obj_0, 14, 0);
        lv_obj_set_style_pad_row(lv_obj_0, 2, 0);
        lv_obj_set_style_pad_column(lv_obj_0, 6, 0);
        lv_obj_set_style_layout(lv_obj_0, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(lv_obj_0, LV_FLEX_FLOW_ROW_WRAP, 0);

        lv_obj_t * title_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(title_label, "title_label");
        lv_label_set_text(title_label, title);
        lv_obj_set_width(title_label, lv_pct(100));
        lv_obj_set_style_text_font(title_label, font_row_title, 0);
        lv_obj_set_style_text_color(title_label, COLOR_TEXT, 0);

        lv_obj_t * subtitle_label = lv_label_create(lv_obj_0);
        lv_obj_set_name(subtitle_label, "subtitle_label");
        lv_label_set_text(subtitle_label, subtitle);
        lv_obj_set_width(subtitle_label, lv_pct(100));
        lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_style_text_font(subtitle_label, font_body, 0);
        lv_obj_set_style_text_color(subtitle_label, COLOR_MUTED, 0);
        lv_obj_set_style_margin_bottom(subtitle_label, 10, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

