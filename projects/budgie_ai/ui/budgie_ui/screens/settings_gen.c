/**
 * @file settings_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "settings_gen.h"
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

lv_obj_t * settings_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_icon_btn;
    static lv_style_t style_icon_btn_pressed;
    static lv_style_t style_info_value;
    static lv_style_t style_danger_btn;
    static lv_style_t style_danger_btn_pressed;
    static lv_style_t style_danger_btn_disabled;
    static lv_style_t style_support_btn;
    static lv_style_t style_support_btn_pressed;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_icon_btn);
        lv_style_init(&style_icon_btn_pressed);
        lv_style_init(&style_info_value);
        lv_style_init(&style_danger_btn);
        lv_style_init(&style_danger_btn_pressed);
        lv_style_init(&style_danger_btn_disabled);
        lv_style_init(&style_support_btn);
        lv_style_init(&style_support_btn_pressed);

        lv_style_set_radius(&style_icon_btn, 999);
        lv_style_set_pad_all(&style_icon_btn, 0);
        lv_style_set_bg_opa(&style_icon_btn, 0);
        lv_style_set_border_width(&style_icon_btn, 0);
        lv_style_set_shadow_width(&style_icon_btn, 0);
        lv_style_set_outline_width(&style_icon_btn, 0);
        lv_style_set_bg_opa(&style_icon_btn_pressed, 255);
        lv_style_set_bg_color(&style_icon_btn_pressed, COLOR_KEY_PRESSED);
        lv_style_set_text_font(&style_info_value, font_row_title);
        lv_style_set_text_color(&style_info_value, COLOR_MUTED);
        lv_style_set_radius(&style_danger_btn, 999);
        lv_style_set_pad_hor(&style_danger_btn, 16);
        lv_style_set_pad_ver(&style_danger_btn, 10);
        lv_style_set_bg_opa(&style_danger_btn, 255);
        lv_style_set_bg_color(&style_danger_btn, COLOR_CARD);
        lv_style_set_border_width(&style_danger_btn, 1);
        lv_style_set_border_color(&style_danger_btn, COLOR_DANGER);
        lv_style_set_shadow_width(&style_danger_btn, 0);
        lv_style_set_outline_width(&style_danger_btn, 0);
        lv_style_set_text_font(&style_danger_btn, font_row_title);
        lv_style_set_text_color(&style_danger_btn, COLOR_DANGER);
        lv_style_set_bg_color(&style_danger_btn_pressed, lv_color_hex(0xfdeeee));
        lv_style_set_border_color(&style_danger_btn_disabled, COLOR_TRACK);
        lv_style_set_text_color(&style_danger_btn_disabled, COLOR_MUTED);
        lv_style_set_radius(&style_support_btn, 999);
        lv_style_set_pad_hor(&style_support_btn, 18);
        lv_style_set_pad_ver(&style_support_btn, 10);
        lv_style_set_border_width(&style_support_btn, 0);
        lv_style_set_outline_width(&style_support_btn, 0);
        lv_style_set_bg_opa(&style_support_btn, 255);
        lv_style_set_bg_color(&style_support_btn, COLOR_BTN_START);
        lv_style_set_bg_grad_color(&style_support_btn, COLOR_BTN_END);
        lv_style_set_bg_grad_dir(&style_support_btn, LV_GRAD_DIR_HOR);
        lv_style_set_shadow_width(&style_support_btn, 10);
        lv_style_set_shadow_opa(&style_support_btn, 70);
        lv_style_set_shadow_color(&style_support_btn, COLOR_BTN_START);
        lv_style_set_shadow_offset_y(&style_support_btn, 2);
        lv_style_set_text_font(&style_support_btn, font_row_title);
        lv_style_set_text_color(&style_support_btn, lv_color_hex(0xffffff));
        lv_style_set_opa(&style_support_btn_pressed, 220);
        lv_style_set_shadow_opa(&style_support_btn_pressed, 20);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "settings_#");
        lv_obj_set_style_bg_color(lv_obj_0, COLOR_BG, 0);
        lv_obj_set_style_bg_opa(lv_obj_0, 255, 0);
        lv_obj_set_style_pad_all(lv_obj_0, 0, 0);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

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

        lv_obj_add_subject_set_int_event(back_btn, &subject_show_support, LV_EVENT_CLICKED, 0);
        lv_obj_add_screen_load_event(back_btn, LV_EVENT_CLICKED, home, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 250, 0);

        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_0, "Settings");
        lv_obj_set_align(lv_label_0, LV_ALIGN_TOP_MID);
        lv_obj_set_y(lv_label_0, 16);
        lv_obj_set_style_text_font(lv_label_0, font_status, 0);
        lv_obj_set_style_text_color(lv_label_0, COLOR_TEXT, 0);

        lv_obj_t * list = lv_obj_create(lv_obj_0);
        lv_obj_set_name(list, "list");
        lv_obj_set_y(list, 56);
        lv_obj_set_width(list, lv_pct(100));
        lv_obj_set_height(list, 424);
        lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_bg_opa(list, 0, 0);
        lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_style_radius(list, 0, 0);
        lv_obj_set_style_pad_hor(list, 16, 0);
        lv_obj_set_style_pad_top(list, 4, 0);
        lv_obj_set_style_pad_bottom(list, 24, 0);
        lv_obj_set_style_pad_row(list, 8, 0);
        lv_obj_set_style_layout(list, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(list, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_t * lv_label_1 = lv_label_create(list);
        lv_label_set_text(lv_label_1, "SOUND");
        lv_obj_add_style(lv_label_1, &style_section_title, 0);

        lv_obj_t * sound_card = lv_obj_create(list);
        lv_obj_set_name(sound_card, "sound_card");
        lv_obj_set_width(sound_card, lv_pct(100));
        lv_obj_set_height(sound_card, LV_SIZE_CONTENT);
        lv_obj_set_flag(sound_card, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_margin_bottom(sound_card, 12, 0);
        lv_obj_add_style(sound_card, &style_settings_card, 0);
        settings_slider_create(sound_card, "Volume", &subject_volume, "%d%%", 0, 100);

        lv_obj_t * lv_obj_1 = lv_obj_create(sound_card);
        lv_obj_set_width(lv_obj_1, lv_pct(100));
        lv_obj_set_height(lv_obj_1, 1);
        lv_obj_add_style(lv_obj_1, &style_row_divider, 0);

        lv_obj_t * settings_row_0 = settings_row_create(sound_card, "Chirp sounds", "Little bird sounds on taps and replies");
        lv_obj_t * lv_switch_0 = lv_switch_create(settings_row_0);
        lv_obj_set_width(lv_switch_0, 52);
        lv_obj_set_height(lv_switch_0, 30);
        lv_obj_bind_checked(lv_switch_0, &subject_chirps);
        lv_obj_add_style(lv_switch_0, &style_switch_track, LV_PART_MAIN);
        lv_obj_add_style(lv_switch_0, &style_switch_on, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(lv_switch_0, &style_switch_knob, LV_PART_KNOB);

        lv_obj_t * lv_label_2 = lv_label_create(list);
        lv_label_set_text(lv_label_2, "DISPLAY");
        lv_obj_add_style(lv_label_2, &style_section_title, 0);

        lv_obj_t * display_card = lv_obj_create(list);
        lv_obj_set_name(display_card, "display_card");
        lv_obj_set_width(display_card, lv_pct(100));
        lv_obj_set_height(display_card, LV_SIZE_CONTENT);
        lv_obj_set_flag(display_card, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_margin_bottom(display_card, 12, 0);
        lv_obj_add_style(display_card, &style_settings_card, 0);
        settings_slider_create(display_card, "Brightness", &subject_brightness, "%d%%", 10, 100);

        lv_obj_t * lv_obj_2 = lv_obj_create(display_card);
        lv_obj_set_width(lv_obj_2, lv_pct(100));
        lv_obj_set_height(lv_obj_2, 1);
        lv_obj_add_style(lv_obj_2, &style_row_divider, 0);

        lv_obj_t * settings_choice_0 = settings_choice_create(display_card, "Screen off after", "Dims the screen when Budgie is idle");
        settings_option_create(settings_choice_0, "30s", &subject_screen_timeout, 0);

        settings_option_create(settings_choice_0, "1m", &subject_screen_timeout, 1);

        settings_option_create(settings_choice_0, "5m", &subject_screen_timeout, 2);

        settings_option_create(settings_choice_0, "Never", &subject_screen_timeout, 3);

        lv_obj_t * lv_obj_3 = lv_obj_create(display_card);
        lv_obj_set_width(lv_obj_3, lv_pct(100));
        lv_obj_set_height(lv_obj_3, 1);
        lv_obj_add_style(lv_obj_3, &style_row_divider, 0);

        lv_obj_t * settings_choice_1 = settings_choice_create(display_card, "Time format", "Clock in the status bar");
        settings_option_create(settings_choice_1, "12 hr", &subject_clock_format, 0);

        settings_option_create(settings_choice_1, "24 hr", &subject_clock_format, 1);

        lv_obj_t * lv_label_3 = lv_label_create(list);
        lv_label_set_text(lv_label_3, "ASSISTANT");
        lv_obj_add_style(lv_label_3, &style_section_title, 0);

        lv_obj_t * assistant_card = lv_obj_create(list);
        lv_obj_set_name(assistant_card, "assistant_card");
        lv_obj_set_width(assistant_card, lv_pct(100));
        lv_obj_set_height(assistant_card, LV_SIZE_CONTENT);
        lv_obj_set_flag(assistant_card, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_margin_bottom(assistant_card, 12, 0);
        lv_obj_add_style(assistant_card, &style_settings_card, 0);
        lv_obj_t * settings_row_1 = settings_row_create(assistant_card, "Kid-safe answers", "Simpler, family-friendly replies");
        lv_obj_t * lv_switch_1 = lv_switch_create(settings_row_1);
        lv_obj_set_width(lv_switch_1, 52);
        lv_obj_set_height(lv_switch_1, 30);
        lv_obj_bind_checked(lv_switch_1, &subject_kid_safe);
        lv_obj_add_style(lv_switch_1, &style_switch_track, LV_PART_MAIN);
        lv_obj_add_style(lv_switch_1, &style_switch_on, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(lv_switch_1, &style_switch_knob, LV_PART_KNOB);

        lv_obj_t * lv_label_4 = lv_label_create(list);
        lv_label_set_text(lv_label_4, "DEVICE");
        lv_obj_add_style(lv_label_4, &style_section_title, 0);

        lv_obj_t * device_card = lv_obj_create(list);
        lv_obj_set_name(device_card, "device_card");
        lv_obj_set_width(device_card, lv_pct(100));
        lv_obj_set_height(device_card, LV_SIZE_CONTENT);
        lv_obj_set_flag(device_card, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_margin_bottom(device_card, 12, 0);
        lv_obj_add_style(device_card, &style_settings_card, 0);
        lv_obj_t * settings_row_2 = settings_row_create(device_card, "Wi-Fi", "Connected network");
        lv_obj_t * lv_label_5 = lv_label_create(settings_row_2);
        lv_label_bind_text(lv_label_5, &subject_wifi_name, NULL);
        lv_obj_add_style(lv_label_5, &style_info_value, 0);

        lv_obj_t * lv_obj_4 = lv_obj_create(device_card);
        lv_obj_set_width(lv_obj_4, lv_pct(100));
        lv_obj_set_height(lv_obj_4, 1);
        lv_obj_add_style(lv_obj_4, &style_row_divider, 0);

        lv_obj_t * settings_row_3 = settings_row_create(device_card, "Web setup", "Open this address in a browser");
        lv_obj_t * lv_label_6 = lv_label_create(settings_row_3);
        lv_label_bind_text(lv_label_6, &subject_web_url, NULL);
        lv_obj_add_style(lv_label_6, &style_info_value, 0);

        lv_obj_t * lv_obj_5 = lv_obj_create(device_card);
        lv_obj_set_width(lv_obj_5, lv_pct(100));
        lv_obj_set_height(lv_obj_5, 1);
        lv_obj_add_style(lv_obj_5, &style_row_divider, 0);

        lv_obj_t * settings_row_4 = settings_row_create(device_card, "Battery", "Charge level");
        lv_obj_t * lv_label_7 = lv_label_create(settings_row_4);
        lv_label_bind_text(lv_label_7, &subject_battery, "%d%%");
        lv_obj_add_style(lv_label_7, &style_info_value, 0);

        lv_obj_t * lv_obj_6 = lv_obj_create(device_card);
        lv_obj_set_width(lv_obj_6, lv_pct(100));
        lv_obj_set_height(lv_obj_6, 1);
        lv_obj_add_style(lv_obj_6, &style_row_divider, 0);

        lv_obj_t * settings_row_5 = settings_row_create(device_card, "Chat history", "Clear the conversation on the chat screen");
        lv_obj_t * clear_history_btn = lv_button_create(settings_row_5);
        lv_obj_set_name(clear_history_btn, "clear_history_btn");
        lv_obj_set_width(clear_history_btn, LV_SIZE_CONTENT);
        lv_obj_set_height(clear_history_btn, LV_SIZE_CONTENT);
        lv_obj_add_style(clear_history_btn, &style_danger_btn, 0);
        lv_obj_add_style(clear_history_btn, &style_danger_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(clear_history_btn, &style_danger_btn_disabled, LV_STATE_DISABLED);
        lv_obj_t * clear_history_label = lv_label_create(clear_history_btn);
        lv_obj_set_name(clear_history_label, "clear_history_label");
        lv_label_set_text(clear_history_label, "Clear");

        lv_obj_add_event_cb(clear_history_btn, settings_clear_history_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * lv_obj_7 = lv_obj_create(device_card);
        lv_obj_set_width(lv_obj_7, lv_pct(100));
        lv_obj_set_height(lv_obj_7, 1);
        lv_obj_add_style(lv_obj_7, &style_row_divider, 0);

        lv_obj_t * settings_row_6 = settings_row_create(device_card, "Power", "Turn Budgie off (the power key always works too)");
        lv_obj_t * power_off_btn = lv_button_create(settings_row_6);
        lv_obj_set_name(power_off_btn, "power_off_btn");
        lv_obj_set_width(power_off_btn, LV_SIZE_CONTENT);
        lv_obj_set_height(power_off_btn, LV_SIZE_CONTENT);
        lv_obj_add_style(power_off_btn, &style_danger_btn, 0);
        lv_obj_add_style(power_off_btn, &style_danger_btn_pressed, LV_STATE_PRESSED);
        lv_obj_t * power_off_label = lv_label_create(power_off_btn);
        lv_obj_set_name(power_off_label, "power_off_label");
        lv_label_set_text(power_off_label, "Power off");

        lv_obj_add_event_cb(power_off_btn, settings_power_off_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * lv_obj_8 = lv_obj_create(device_card);
        lv_obj_set_width(lv_obj_8, lv_pct(100));
        lv_obj_set_height(lv_obj_8, 1);
        lv_obj_add_style(lv_obj_8, &style_row_divider, 0);

        lv_obj_t * settings_row_7 = settings_row_create(device_card, "About Budgie", "Software version");
        lv_obj_t * lv_label_8 = lv_label_create(settings_row_7);
        lv_label_bind_text(lv_label_8, &subject_app_version, "v%s");
        lv_obj_add_style(lv_label_8, &style_info_value, 0);

        lv_obj_t * lv_label_9 = lv_label_create(list);
        lv_label_set_text(lv_label_9, "SUPPORT");
        lv_obj_add_style(lv_label_9, &style_section_title, 0);

        lv_obj_t * support_card = lv_obj_create(list);
        lv_obj_set_name(support_card, "support_card");
        lv_obj_set_width(support_card, lv_pct(100));
        lv_obj_set_height(support_card, LV_SIZE_CONTENT);
        lv_obj_set_flag(support_card, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(support_card, &style_settings_card, 0);
        lv_obj_t * settings_row_8 = settings_row_create(support_card, "Support Budgie", "Enjoying Budgie? Buy me a coffee");
        lv_obj_t * support_btn = lv_button_create(settings_row_8);
        lv_obj_set_name(support_btn, "support_btn");
        lv_obj_set_width(support_btn, LV_SIZE_CONTENT);
        lv_obj_set_height(support_btn, LV_SIZE_CONTENT);
        lv_obj_set_flag(support_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_add_style(support_btn, &style_support_btn, 0);
        lv_obj_add_style(support_btn, &style_support_btn_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_label_10 = lv_label_create(support_btn);
        lv_label_set_text(lv_label_10, "Support");

        lv_obj_add_subject_set_int_event(support_btn, &subject_show_support, LV_EVENT_CLICKED, 1);

        lv_obj_t * credit = lv_label_create(list);
        lv_obj_set_name(credit, "credit");
        lv_label_set_text(credit, "Created by nishad2m8");
        lv_obj_set_width(credit, lv_pct(100));
        lv_obj_set_style_text_align(credit, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_margin_top(credit, 8, 0);
        lv_obj_set_style_text_font(credit, font_body, 0);
        lv_obj_set_style_text_color(credit, COLOR_MUTED, 0);

        lv_obj_t * support_overlay = lv_obj_create(lv_obj_0);
        lv_obj_set_name(support_overlay, "support_overlay");
        lv_obj_set_width(support_overlay, lv_pct(100));
        lv_obj_set_height(support_overlay, lv_pct(100));
        lv_obj_set_flag(support_overlay, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_bg_opa(support_overlay, 150, 0);
        lv_obj_set_style_bg_color(support_overlay, COLOR_TEXT, 0);
        lv_obj_set_style_border_width(support_overlay, 0, 0);
        lv_obj_set_style_radius(support_overlay, 0, 0);
        lv_obj_set_style_pad_all(support_overlay, 0, 0);
        lv_obj_bind_flag_if_not_eq(support_overlay, &subject_show_support, LV_OBJ_FLAG_HIDDEN, 1);
        lv_obj_add_subject_set_int_event(support_overlay, &subject_show_support, LV_EVENT_CLICKED, 0);
        lv_obj_t * support_popup = lv_obj_create(support_overlay);
        lv_obj_set_name(support_popup, "support_popup");
        lv_obj_set_width(support_popup, 300);
        lv_obj_set_height(support_popup, LV_SIZE_CONTENT);
        lv_obj_set_align(support_popup, LV_ALIGN_CENTER);
        lv_obj_set_flag(support_popup, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_radius(support_popup, 24, 0);
        lv_obj_set_style_bg_opa(support_popup, 255, 0);
        lv_obj_set_style_bg_color(support_popup, COLOR_CARD, 0);
        lv_obj_set_style_border_width(support_popup, 0, 0);
        lv_obj_set_style_pad_hor(support_popup, 24, 0);
        lv_obj_set_style_pad_top(support_popup, 22, 0);
        lv_obj_set_style_pad_bottom(support_popup, 20, 0);
        lv_obj_set_style_pad_row(support_popup, 6, 0);
        lv_obj_set_style_shadow_width(support_popup, 40, 0);
        lv_obj_set_style_shadow_opa(support_popup, 80, 0);
        lv_obj_set_style_shadow_color(support_popup, COLOR_TEXT, 0);
        lv_obj_set_style_shadow_offset_y(support_popup, 8, 0);
        lv_obj_set_style_layout(support_popup, LV_LAYOUT_FLEX, 0);
        lv_obj_set_style_flex_flow(support_popup, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_style_flex_cross_place(support_popup, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_t * lv_label_11 = lv_label_create(support_popup);
        lv_label_set_text(lv_label_11, "Support Budgie");
        lv_obj_set_style_text_font(lv_label_11, font_status, 0);
        lv_obj_set_style_text_color(lv_label_11, COLOR_TEXT, 0);

        lv_obj_t * lv_label_12 = lv_label_create(support_popup);
        lv_label_set_text(lv_label_12, "Scan to buy me a coffee");
        lv_obj_set_style_text_font(lv_label_12, font_body, 0);
        lv_obj_set_style_text_color(lv_label_12, COLOR_MUTED, 0);
        lv_obj_set_style_margin_bottom(lv_label_12, 10, 0);

        lv_obj_t * support_qr = lv_qrcode_create(support_popup);
        lv_obj_set_name(support_qr, "support_qr");
        lv_qrcode_set_size(support_qr, 180);
        lv_qrcode_set_quiet_zone(support_qr, true);
        lv_qrcode_set_dark_color(support_qr, COLOR_TEXT);
        lv_qrcode_set_light_color(support_qr, lv_color_hex(0xffffff));
        lv_qrcode_set_data(support_qr, "https://buymeacoffee.com/nishad2m8");

        lv_obj_t * lv_label_13 = lv_label_create(support_popup);
        lv_label_set_text(lv_label_13, "buymeacoffee.com/nishad2m8");
        lv_obj_set_style_text_font(lv_label_13, font_key_digit, 0);
        lv_obj_set_style_text_color(lv_label_13, COLOR_MUTED, 0);
        lv_obj_set_style_margin_top(lv_label_13, 6, 0);

        lv_obj_t * support_close_btn = lv_button_create(support_popup);
        lv_obj_set_name(support_close_btn, "support_close_btn");
        lv_obj_set_width(support_close_btn, lv_pct(100));
        lv_obj_set_height(support_close_btn, 44);
        lv_obj_set_flag(support_close_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE, false);
        lv_obj_set_style_margin_top(support_close_btn, 12, 0);
        lv_obj_add_style(support_close_btn, &style_support_btn, 0);
        lv_obj_add_style(support_close_btn, &style_support_btn_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_label_14 = lv_label_create(support_close_btn);
        lv_label_set_text(lv_label_14, "Close");
        lv_obj_set_align(lv_label_14, LV_ALIGN_CENTER);

        lv_obj_add_subject_set_int_event(support_close_btn, &subject_show_support, LV_EVENT_CLICKED, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

