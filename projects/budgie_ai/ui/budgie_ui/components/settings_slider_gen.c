/**
 * @file settings_slider_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "settings_slider_gen.h"
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

lv_obj_t * settings_slider_create(lv_obj_t * parent, const char * title, lv_subject_t * subject, const char * fmt, int32_t min, int32_t max)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "settings_slider_#");
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, LV_SIZE_CONTENT);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, 0, 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_hor(lv_obj_0, 16, 0);
        lv_obj_set_style_pad_top(lv_obj_0, 14, 0);
        lv_obj_set_style_pad_bottom(lv_obj_0, 20, 0);
        lv_obj_set_style_pad_row(lv_obj_0, 14, 0);
        lv_obj_set_style_layout(lv_obj_0, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN, 0);

        lv_obj_t * header = lv_obj_create(lv_obj_0);
        lv_obj_set_name(header, "header");
        lv_obj_set_width(header, lv_pct(100));
        lv_obj_set_height(header, LV_SIZE_CONTENT);
        lv_obj_set_flag(header, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(header, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_bg_opa(header, 0, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_set_style_layout(header, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(header, LV_FLEX_FLOW_ROW, 0);
        lv_obj_t * title_label = lv_label_create(header);
        lv_obj_set_name(title_label, "title_label");
        lv_label_set_text(title_label, title);
        lv_obj_set_flex_grow(title_label, 1);
        lv_obj_set_style_text_font(title_label, font_row_title, 0);
        lv_obj_set_style_text_color(title_label, COLOR_TEXT, 0);

        lv_obj_t * value_label = lv_label_create(header);
        lv_obj_set_name(value_label, "value_label");
        lv_label_bind_text(value_label, subject, fmt);
        lv_obj_set_style_text_font(value_label, font_row_title, 0);
        lv_obj_set_style_text_color(value_label, COLOR_BTN_START, 0);

        lv_obj_t * slider = lv_slider_create(lv_obj_0);
        lv_obj_set_name(slider, "slider");
        lv_obj_set_width(slider, lv_pct(100));
        lv_obj_set_height(slider, 8);
        lv_slider_set_min_value(slider, min);
        lv_slider_set_max_value(slider, max);
        lv_slider_bind_value(slider, subject);
        lv_obj_set_style_margin_left(slider, 10, 0);
        lv_obj_set_style_margin_right(slider, 10, 0);
        lv_obj_set_ext_click_area(slider, 12);
        lv_obj_add_style(slider, &style_slider_track, LV_PART_MAIN);
        lv_obj_add_style(slider, &style_slider_fill, LV_PART_INDICATOR);
        lv_obj_add_style(slider, &style_slider_knob, LV_PART_KNOB);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

