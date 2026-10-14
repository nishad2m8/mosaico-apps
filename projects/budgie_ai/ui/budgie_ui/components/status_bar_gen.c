/**
 * @file status_bar_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "status_bar_gen.h"
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

lv_obj_t * status_bar_create(lv_obj_t * parent, const char * title)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_clear;
    static lv_style_t style_row;
    static lv_style_t style_battery_track;
    static lv_style_t style_battery_fill;
    static lv_style_t style_battery_fill_low;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_clear);
        lv_style_init(&style_row);
        lv_style_init(&style_battery_track);
        lv_style_init(&style_battery_fill);
        lv_style_init(&style_battery_fill_low);

        lv_style_set_bg_opa(&style_clear, 0);
        lv_style_set_border_width(&style_clear, 0);
        lv_style_set_pad_all(&style_clear, 0);
        lv_style_set_radius(&style_clear, 0);
        lv_style_set_layout(&style_row, LV_LAYOUT_FLEX);
        lv_style_set_flex_flow(&style_row, LV_FLEX_FLOW_ROW);
        lv_style_set_flex_main_place(&style_row, LV_FLEX_ALIGN_CENTER);
        lv_style_set_flex_cross_place(&style_row, LV_FLEX_ALIGN_CENTER);
        lv_style_set_flex_track_place(&style_row, LV_FLEX_ALIGN_CENTER);
        lv_style_set_bg_opa(&style_battery_track, 0);
        lv_style_set_border_width(&style_battery_track, 0);
        lv_style_set_pad_all(&style_battery_track, 0);
        lv_style_set_radius(&style_battery_track, 0);
        lv_style_set_anim_duration(&style_battery_track, 0);
        lv_style_set_bg_opa(&style_battery_fill, 255);
        lv_style_set_bg_color(&style_battery_fill, COLOR_ICON);
        lv_style_set_radius(&style_battery_fill, 1);
        lv_style_set_bg_color(&style_battery_fill_low, COLOR_BATTERY_LOW);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "status_bar_#");
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, 64);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_pad_hor(lv_obj_0, 20, 0);

        lv_obj_add_style(lv_obj_0, &style_clear, 0);
        lv_obj_t * left = lv_obj_create(lv_obj_0);
        lv_obj_set_name(left, "left");
        lv_obj_set_width(left, LV_SIZE_CONTENT);
        lv_obj_set_height(left, LV_SIZE_CONTENT);
        lv_obj_set_align(left, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_pad_column(left, 12, 0);
        lv_obj_set_flag(left, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(left, &style_clear, 0);
        lv_obj_add_style(left, &style_row, 0);
        lv_obj_t * wifi = lv_obj_create(left);
        lv_obj_set_name(wifi, "wifi");
        lv_obj_set_width(wifi, 28);
        lv_obj_set_height(wifi, 20);
        lv_obj_set_flag(wifi, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(wifi, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_add_style(wifi, &style_clear, 0);
        lv_obj_t * lv_image_0 = lv_image_create(wifi);
        lv_image_set_src(lv_image_0, img_wifi_0);
        lv_obj_bind_flag_if_not_eq(lv_image_0, &subject_wifi_level, LV_OBJ_FLAG_HIDDEN, 0);

        lv_obj_t * lv_image_1 = lv_image_create(wifi);
        lv_image_set_src(lv_image_1, img_wifi_1);
        lv_obj_bind_flag_if_not_eq(lv_image_1, &subject_wifi_level, LV_OBJ_FLAG_HIDDEN, 1);

        lv_obj_t * lv_image_2 = lv_image_create(wifi);
        lv_image_set_src(lv_image_2, img_wifi_2);
        lv_obj_bind_flag_if_not_eq(lv_image_2, &subject_wifi_level, LV_OBJ_FLAG_HIDDEN, 2);

        lv_obj_t * lv_image_3 = lv_image_create(wifi);
        lv_image_set_src(lv_image_3, img_wifi_3);
        lv_obj_bind_flag_if_not_eq(lv_image_3, &subject_wifi_level, LV_OBJ_FLAG_HIDDEN, 3);

        lv_obj_t * lv_obj_1 = lv_obj_create(left);
        lv_obj_set_width(lv_obj_1, 1);
        lv_obj_set_height(lv_obj_1, 26);
        lv_obj_set_style_bg_opa(lv_obj_1, 255, 0);
        lv_obj_set_style_bg_color(lv_obj_1, COLOR_DIVIDER, 0);
        lv_obj_set_style_border_width(lv_obj_1, 0, 0);
        lv_obj_set_style_radius(lv_obj_1, 0, 0);

        lv_obj_t * lv_label_0 = lv_label_create(left);
        lv_label_bind_text(lv_label_0, &subject_time, NULL);
        lv_obj_add_style(lv_label_0, &style_status_text, 0);

        lv_obj_t * brand = lv_obj_create(lv_obj_0);
        lv_obj_set_name(brand, "brand");
        lv_obj_set_width(brand, LV_SIZE_CONTENT);
        lv_obj_set_height(brand, LV_SIZE_CONTENT);
        lv_obj_set_align(brand, LV_ALIGN_CENTER);
        lv_obj_set_style_pad_column(brand, 10, 0);
        lv_obj_set_flag(brand, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(brand, &style_clear, 0);
        lv_obj_add_style(brand, &style_row, 0);
        lv_obj_t * title_label = lv_label_create(brand);
        lv_obj_set_name(title_label, "title_label");
        lv_label_set_text(title_label, title);
        lv_obj_set_style_text_font(title_label, font_title, 0);
        lv_obj_set_style_text_color(title_label, COLOR_TEXT, 0);

        lv_obj_t * right = lv_obj_create(lv_obj_0);
        lv_obj_set_name(right, "right");
        lv_obj_set_width(right, LV_SIZE_CONTENT);
        lv_obj_set_height(right, LV_SIZE_CONTENT);
        lv_obj_set_align(right, LV_ALIGN_RIGHT_MID);
        lv_obj_set_style_pad_column(right, 12, 0);
        lv_obj_set_flag(right, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(right, &style_clear, 0);
        lv_obj_add_style(right, &style_row, 0);
        lv_obj_t * battery = lv_obj_create(right);
        lv_obj_set_name(battery, "battery");
        lv_obj_set_width(battery, 31);
        lv_obj_set_height(battery, 18);
        lv_obj_set_flag(battery, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_flag(battery, LV_OBJ_FLAG_CLICKABLE, false);
        lv_obj_add_style(battery, &style_clear, 0);
        lv_obj_t * lv_image_4 = lv_image_create(battery);
        lv_image_set_src(lv_image_4, img_battery_frame);
        lv_obj_set_x(lv_image_4, 0);
        lv_obj_set_y(lv_image_4, 0);

        lv_obj_t * battery_fill = lv_bar_create(battery);
        lv_obj_set_name(battery_fill, "battery_fill");
        lv_obj_set_x(battery_fill, 4);
        lv_obj_set_y(battery_fill, 4);
        lv_obj_set_width(battery_fill, 20);
        lv_obj_set_height(battery_fill, 10);
        lv_bar_set_min_value(battery_fill, 0);
        lv_bar_set_max_value(battery_fill, 100);
        lv_bar_bind_value(battery_fill, &subject_battery);
        lv_obj_add_style(battery_fill, &style_battery_track, 0);
        lv_obj_add_style(battery_fill, &style_battery_fill, LV_PART_INDICATOR);
        lv_obj_add_style(battery_fill, &style_battery_fill_low, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_bind_state_if_le(battery_fill, &subject_battery, LV_STATE_CHECKED, BATTERY_LOW_LEVEL);

        lv_obj_t * lv_label_2 = lv_label_create(right);
        lv_label_bind_text(lv_label_2, &subject_battery, "%d%%");
        lv_obj_add_style(lv_label_2, &style_status_text, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(right);
        lv_obj_set_width(lv_obj_2, 1);
        lv_obj_set_height(lv_obj_2, 26);
        lv_obj_set_style_bg_opa(lv_obj_2, 255, 0);
        lv_obj_set_style_bg_color(lv_obj_2, COLOR_DIVIDER, 0);
        lv_obj_set_style_border_width(lv_obj_2, 0, 0);
        lv_obj_set_style_radius(lv_obj_2, 0, 0);

        lv_obj_t * settings = lv_image_create(right);
        lv_obj_set_name(settings, "settings");
        lv_image_set_src(settings, img_icon_settings);
        lv_obj_set_flag(settings, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_ext_click_area(settings, 10);
        lv_obj_add_screen_create_event(settings, LV_EVENT_CLICKED, settings_create, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, 250, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

