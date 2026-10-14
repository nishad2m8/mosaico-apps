/**
 * @file speech_bubble_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "speech_bubble_gen.h"
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

lv_obj_t * speech_bubble_create(lv_obj_t * parent, const char * title, const char * text)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_bubble_shape;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_bubble_shape);

        lv_style_set_bg_opa(&style_bubble_shape, 255);
        lv_style_set_bg_color(&style_bubble_shape, COLOR_BUBBLE_BG);
        lv_style_set_border_width(&style_bubble_shape, 1);
        lv_style_set_border_color(&style_bubble_shape, COLOR_BUBBLE_BORDER);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "speech_bubble_#");
        lv_obj_set_width(lv_obj_0, 300);
        lv_obj_set_height(lv_obj_0, LV_SIZE_CONTENT);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(lv_obj_0, 0, 0);
        lv_obj_set_style_border_width(lv_obj_0, 0, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_style_radius(lv_obj_0, 0, 0);

        lv_obj_t * tail = lv_obj_create(lv_obj_0);
        lv_obj_set_name(tail, "tail");
        lv_obj_set_width(tail, 14);
        lv_obj_set_height(tail, 14);
        lv_obj_set_align(tail, LV_ALIGN_TOP_LEFT);
        lv_obj_set_x(tail, 40);
        lv_obj_set_y(tail, 3);
        lv_obj_set_flag(tail, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(tail, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_radius(tail, 2, 0);
        lv_obj_set_style_transform_rotation(tail, 450, 0);
        lv_obj_set_style_transform_pivot_x(tail, 7, 0);
        lv_obj_set_style_transform_pivot_y(tail, 7, 0);
        lv_obj_add_style(tail, &style_bubble_shape, 0);

        lv_obj_t * body = lv_obj_create(lv_obj_0);
        lv_obj_set_name(body, "body");
        lv_obj_set_y(body, 10);
        lv_obj_set_width(body, lv_pct(100));
        lv_obj_set_height(body, LV_SIZE_CONTENT);
        lv_obj_set_flag(body, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(body, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_set_style_radius(body, 32, 0);
        lv_obj_set_style_pad_hor(body, 20, 0);
        lv_obj_set_style_pad_ver(body, 14, 0);
        lv_obj_set_style_pad_row(body, 4, 0);
        lv_obj_set_style_layout(body, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(body, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_style_flex_cross_place(body, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_track_place(body, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_add_style(body, &style_bubble_shape, 0);
        lv_obj_t * title_label = lv_label_create(body);
        lv_obj_set_name(title_label, "title_label");
        lv_label_set_text(title_label, title);
        lv_obj_set_style_text_font(title_label, font_bubble_title, 0);
        lv_obj_set_style_text_color(title_label, COLOR_TEXT, 0);

        lv_obj_t * text_label = lv_label_create(body);
        lv_obj_set_name(text_label, "text_label");
        lv_label_set_text(text_label, text);
        lv_obj_set_style_text_font(text_label, font_bubble_text, 0);
        lv_obj_set_style_text_color(text_label, COLOR_MUTED, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

