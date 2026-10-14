/**
 * @file budgie_ui_gen.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "budgie_ui_gen.h"

#if defined(LV_USE_XML) && LV_USE_XML
#endif /* LV_USE_XML */

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void check_font(lv_font_t ** font, const char * name);

/**********************
 *  STATIC VARIABLES
 **********************/

static uint32_t budgie_ui_target = BUDGIE_UI_TARGET_ALL;

/*----------------
 * Translations
 *----------------*/

#ifndef LV_EDITOR_PREVIEW
    static const char * translation_languages[] = {"en", "de", NULL};
    static const char * translation_tags[] = {"dog", "cat", "house", NULL};
    static const char * translation_texts[] = {
        "This is a dog", "Das ist ein Hund", /* dog */
        "A curious little cat", "Eine neugierige kleine Katze", /* cat */
        "The house is cozy and warm", "Das Haus ist gemütlich und warm", /* house */
    };
#endif

/**********************
 *  GLOBAL VARIABLES
 **********************/

/*--------------------
 *  Permanent screens
 *-------------------*/

lv_obj_t * chat = NULL;
lv_obj_t * home = NULL;

/*----------------
 * Fonts
 *----------------*/

lv_font_t * font_title;
extern lv_font_t font_title_data;
lv_font_t * font_status;
extern lv_font_t font_status_data;
lv_font_t * font_body;
extern lv_font_t font_body_data;
lv_font_t * font_bubble_title;
extern lv_font_t font_bubble_title_data;
lv_font_t * font_bubble_text;
extern lv_font_t font_bubble_text_data;
lv_font_t * font_key_digit;
extern lv_font_t font_key_digit_data;
lv_font_t * font_row_title;
extern lv_font_t font_row_title_data;

/*----------------
 * Images
 *----------------*/

/* Targets: any */
const void * img_wifi_0 = NULL;
extern const void * img_wifi_0_data;
const void * img_wifi_1 = NULL;
extern const void * img_wifi_1_data;
const void * img_wifi_2 = NULL;
extern const void * img_wifi_2_data;
const void * img_wifi_3 = NULL;
extern const void * img_wifi_3_data;
const void * img_battery_frame = NULL;
extern const void * img_battery_frame_data;
const void * img_budgie_idle = NULL;
extern const void * img_budgie_idle_data;
const void * img_budgie_listening = NULL;
extern const void * img_budgie_listening_data;
const void * img_budgie_thinking = NULL;
extern const void * img_budgie_thinking_data;
const void * img_budgie_talking = NULL;
extern const void * img_budgie_talking_data;
const void * img_budgie_avatar = NULL;
extern const void * img_budgie_avatar_data;
const void * img_icon_mic = NULL;
extern const void * img_icon_mic_data;
const void * img_icon_camera = NULL;
extern const void * img_icon_camera_data;
const void * img_icon_keyboard = NULL;
extern const void * img_icon_keyboard_data;
const void * img_icon_settings = NULL;
extern const void * img_icon_settings_data;
const void * img_icon_back = NULL;
extern const void * img_icon_back_data;
const void * img_icon_back_light = NULL;
extern const void * img_icon_back_light_data;
const void * img_icon_photo = NULL;
extern const void * img_icon_photo_data;
const void * img_icon_camera_small = NULL;
extern const void * img_icon_camera_small_data;
const void * img_icon_close_small = NULL;
extern const void * img_icon_close_small_data;
const void * img_icon_backspace = NULL;
extern const void * img_icon_backspace_data;
const void * img_icon_close = NULL;
extern const void * img_icon_close_data;
const void * img_icon_send = NULL;
extern const void * img_icon_send_data;
const void * img_icon_send_large = NULL;
extern const void * img_icon_send_large_data;

/*----------------
 * Global styles
 *----------------*/

lv_style_t style_status_text;
lv_style_t style_round_btn;
lv_style_t style_round_btn_pressed;
lv_style_t style_key;
lv_style_t style_key_pressed;
lv_style_t style_key_fn;
lv_style_t style_settings_card;
lv_style_t style_section_title;
lv_style_t style_row_divider;
lv_style_t style_switch_track;
lv_style_t style_switch_on;
lv_style_t style_switch_knob;
lv_style_t style_slider_track;
lv_style_t style_slider_fill;
lv_style_t style_slider_knob;
lv_style_t style_round_btn_disabled;

/*----------------
 * Subjects
 *----------------*/

lv_subject_t subject_wifi_level;
lv_subject_t subject_battery;
lv_subject_t subject_time;
lv_subject_t subject_budgie_mode;
lv_subject_t subject_photo_taken;
lv_subject_t subject_camera_origin;
lv_subject_t subject_chat_has_photo;
lv_subject_t subject_volume;
lv_subject_t subject_chirps;
lv_subject_t subject_brightness;
lv_subject_t subject_screen_timeout;
lv_subject_t subject_clock_format;
lv_subject_t subject_kid_safe;
lv_subject_t subject_wifi_name;
lv_subject_t subject_web_url;
lv_subject_t subject_app_version;
lv_subject_t subject_show_support;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void budgie_ui_init_gen(const char * asset_path)
{
    /* When running from the editor the theme set from the XML should overwrite this */
#if !defined(LV_EDITOR_PREVIEW)
#if LV_USE_THEME_SIMPLE
    lv_display_t * disp = lv_display_get_default();
    lv_theme_t * th = lv_theme_simple_init(disp);
    lv_display_set_theme(disp, th);
#else
    LV_LOG_WARN("Simple theme is selected in project.xml but LV_USE_THEME_SIMPLE is disabled");
#endif
#endif /*LV_EDITOR_PREVIEW*/


    /*----------------
     * Fonts
     *----------------*/

    /* Targets: any */

    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        if (!font_title) {
            /* font_title */
            /* get font 'font_title' from a C array */
            font_title = &font_title_data;

        }
        if (!font_status) {
            /* font_status */
            /* get font 'font_status' from a C array */
            font_status = &font_status_data;

        }
        if (!font_body) {
            /* font_body */
            /* get font 'font_body' from a C array */
            font_body = &font_body_data;

        }
        if (!font_bubble_title) {
            /* font_bubble_title */
            /* get font 'font_bubble_title' from a C array */
            font_bubble_title = &font_bubble_title_data;

        }
        if (!font_bubble_text) {
            /* font_bubble_text */
            /* get font 'font_bubble_text' from a C array */
            font_bubble_text = &font_bubble_text_data;

        }
        if (!font_key_digit) {
            /* font_key_digit */
            /* get font 'font_key_digit' from a C array */
            font_key_digit = &font_key_digit_data;

        }
        if (!font_row_title) {
            /* font_row_title */
            /* get font 'font_row_title' from a C array */
            font_row_title = &font_row_title_data;

        }
    }
    #endif

    /*----------------
     * Images
     *----------------*/

    /* Targets: any */
    #if BUDGIE_UI_CHECK_COMPILE_TARGET(BUDGIE_UI_TARGET_ALL)
    if (budgie_ui_check_target(BUDGIE_UI_TARGET_ALL)) {
        /* img_wifi_0 */
        if (!img_wifi_0) {
            img_wifi_0 = &img_wifi_0_data;
        }
        /* img_wifi_1 */
        if (!img_wifi_1) {
            img_wifi_1 = &img_wifi_1_data;
        }
        /* img_wifi_2 */
        if (!img_wifi_2) {
            img_wifi_2 = &img_wifi_2_data;
        }
        /* img_wifi_3 */
        if (!img_wifi_3) {
            img_wifi_3 = &img_wifi_3_data;
        }
        /* img_battery_frame */
        if (!img_battery_frame) {
            img_battery_frame = &img_battery_frame_data;
        }
        /* img_budgie_idle */
        if (!img_budgie_idle) {
            img_budgie_idle = &img_budgie_idle_data;
        }
        /* img_budgie_listening */
        if (!img_budgie_listening) {
            img_budgie_listening = &img_budgie_listening_data;
        }
        /* img_budgie_thinking */
        if (!img_budgie_thinking) {
            img_budgie_thinking = &img_budgie_thinking_data;
        }
        /* img_budgie_talking */
        if (!img_budgie_talking) {
            img_budgie_talking = &img_budgie_talking_data;
        }
        /* img_budgie_avatar */
        if (!img_budgie_avatar) {
            img_budgie_avatar = &img_budgie_avatar_data;
        }
        /* img_icon_mic */
        if (!img_icon_mic) {
            img_icon_mic = &img_icon_mic_data;
        }
        /* img_icon_camera */
        if (!img_icon_camera) {
            img_icon_camera = &img_icon_camera_data;
        }
        /* img_icon_keyboard */
        if (!img_icon_keyboard) {
            img_icon_keyboard = &img_icon_keyboard_data;
        }
        /* img_icon_settings */
        if (!img_icon_settings) {
            img_icon_settings = &img_icon_settings_data;
        }
        /* img_icon_back */
        if (!img_icon_back) {
            img_icon_back = &img_icon_back_data;
        }
        /* img_icon_back_light */
        if (!img_icon_back_light) {
            img_icon_back_light = &img_icon_back_light_data;
        }
        /* img_icon_photo */
        if (!img_icon_photo) {
            img_icon_photo = &img_icon_photo_data;
        }
        /* img_icon_camera_small */
        if (!img_icon_camera_small) {
            img_icon_camera_small = &img_icon_camera_small_data;
        }
        /* img_icon_close_small */
        if (!img_icon_close_small) {
            img_icon_close_small = &img_icon_close_small_data;
        }
        /* img_icon_backspace */
        if (!img_icon_backspace) {
            img_icon_backspace = &img_icon_backspace_data;
        }
        /* img_icon_close */
        if (!img_icon_close) {
            img_icon_close = &img_icon_close_data;
        }
        /* img_icon_send */
        if (!img_icon_send) {
            img_icon_send = &img_icon_send_data;
        }
        /* img_icon_send_large */
        if (!img_icon_send_large) {
            img_icon_send_large = &img_icon_send_large_data;
        }
    }
    #endif

    /*----------------
     * Global styles
     *----------------*/

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_status_text);
        lv_style_init(&style_round_btn);
        lv_style_init(&style_round_btn_pressed);
        lv_style_init(&style_key);
        lv_style_init(&style_key_pressed);
        lv_style_init(&style_key_fn);
        lv_style_init(&style_settings_card);
        lv_style_init(&style_section_title);
        lv_style_init(&style_row_divider);
        lv_style_init(&style_switch_track);
        lv_style_init(&style_switch_on);
        lv_style_init(&style_switch_knob);
        lv_style_init(&style_slider_track);
        lv_style_init(&style_slider_fill);
        lv_style_init(&style_slider_knob);
        lv_style_init(&style_round_btn_disabled);

        lv_style_set_text_font(&style_status_text, font_status);
        lv_style_set_text_color(&style_status_text, COLOR_ICON);
        lv_style_set_radius(&style_round_btn, 999);
        lv_style_set_pad_all(&style_round_btn, 0);
        lv_style_set_border_width(&style_round_btn, 0);
        lv_style_set_bg_opa(&style_round_btn, 255);
        lv_style_set_bg_color(&style_round_btn, COLOR_BTN_START);
        lv_style_set_bg_grad_color(&style_round_btn, COLOR_BTN_END);
        lv_style_set_bg_grad_dir(&style_round_btn, LV_GRAD_DIR_HOR);
        lv_style_set_outline_width(&style_round_btn, 8);
        lv_style_set_outline_pad(&style_round_btn, 0);
        lv_style_set_outline_color(&style_round_btn, COLOR_BTN_HALO);
        lv_style_set_outline_opa(&style_round_btn, 200);
        lv_style_set_shadow_width(&style_round_btn, 24);
        lv_style_set_shadow_offset_y(&style_round_btn, 6);
        lv_style_set_shadow_color(&style_round_btn, COLOR_BTN_START);
        lv_style_set_shadow_opa(&style_round_btn, 60);
        lv_style_set_opa(&style_round_btn_pressed, 220);
        lv_style_set_shadow_opa(&style_round_btn_pressed, 20);
        lv_style_set_radius(&style_key, 14);
        lv_style_set_pad_all(&style_key, 0);
        lv_style_set_bg_opa(&style_key, 255);
        lv_style_set_bg_color(&style_key, COLOR_CARD);
        lv_style_set_border_width(&style_key, 1);
        lv_style_set_border_color(&style_key, COLOR_BUBBLE_BORDER);
        lv_style_set_shadow_width(&style_key, 0);
        lv_style_set_outline_width(&style_key, 0);
        lv_style_set_bg_color(&style_key_pressed, COLOR_KEY_PRESSED);
        lv_style_set_bg_color(&style_key_fn, COLOR_KEY_FN);
        lv_style_set_radius(&style_settings_card, 20);
        lv_style_set_pad_all(&style_settings_card, 0);
        lv_style_set_bg_opa(&style_settings_card, 255);
        lv_style_set_bg_color(&style_settings_card, COLOR_CARD);
        lv_style_set_border_width(&style_settings_card, 1);
        lv_style_set_border_color(&style_settings_card, COLOR_BUBBLE_BORDER);
        lv_style_set_layout(&style_settings_card, LV_LAYOUT_FLEX);
        lv_style_set_flex_flow(&style_settings_card, LV_FLEX_FLOW_COLUMN);
        lv_style_set_text_font(&style_section_title, font_key_digit);
        lv_style_set_text_color(&style_section_title, COLOR_MUTED);
        lv_style_set_text_letter_space(&style_section_title, 1);
        lv_style_set_pad_left(&style_section_title, 8);
        lv_style_set_bg_opa(&style_row_divider, 255);
        lv_style_set_bg_color(&style_row_divider, COLOR_TRACK);
        lv_style_set_border_width(&style_row_divider, 0);
        lv_style_set_radius(&style_row_divider, 0);
        lv_style_set_pad_all(&style_row_divider, 0);
        lv_style_set_bg_opa(&style_switch_track, 255);
        lv_style_set_bg_color(&style_switch_track, COLOR_TRACK);
        lv_style_set_radius(&style_switch_track, 999);
        lv_style_set_border_width(&style_switch_track, 0);
        lv_style_set_bg_opa(&style_switch_on, 255);
        lv_style_set_radius(&style_switch_on, 999);
        lv_style_set_bg_color(&style_switch_on, COLOR_BTN_START);
        lv_style_set_bg_grad_color(&style_switch_on, COLOR_BTN_END);
        lv_style_set_bg_grad_dir(&style_switch_on, LV_GRAD_DIR_HOR);
        lv_style_set_bg_opa(&style_switch_knob, 255);
        lv_style_set_bg_color(&style_switch_knob, lv_color_hex(0xffffff));
        lv_style_set_radius(&style_switch_knob, 999);
        lv_style_set_pad_all(&style_switch_knob, -3);
        lv_style_set_shadow_width(&style_switch_knob, 6);
        lv_style_set_shadow_opa(&style_switch_knob, 50);
        lv_style_set_shadow_color(&style_switch_knob, COLOR_ICON);
        lv_style_set_shadow_offset_y(&style_switch_knob, 1);
        lv_style_set_bg_opa(&style_slider_track, 255);
        lv_style_set_bg_color(&style_slider_track, COLOR_TRACK);
        lv_style_set_radius(&style_slider_track, 999);
        lv_style_set_border_width(&style_slider_track, 0);
        lv_style_set_pad_all(&style_slider_track, 0);
        lv_style_set_bg_opa(&style_slider_fill, 255);
        lv_style_set_bg_color(&style_slider_fill, COLOR_BTN_START);
        lv_style_set_bg_grad_color(&style_slider_fill, COLOR_BTN_END);
        lv_style_set_bg_grad_dir(&style_slider_fill, LV_GRAD_DIR_HOR);
        lv_style_set_radius(&style_slider_fill, 999);
        lv_style_set_bg_opa(&style_slider_knob, 255);
        lv_style_set_bg_color(&style_slider_knob, lv_color_hex(0xffffff));
        lv_style_set_radius(&style_slider_knob, 999);
        lv_style_set_pad_all(&style_slider_knob, 7);
        lv_style_set_border_width(&style_slider_knob, 2);
        lv_style_set_border_color(&style_slider_knob, COLOR_BTN_START);
        lv_style_set_shadow_width(&style_slider_knob, 8);
        lv_style_set_shadow_opa(&style_slider_knob, 50);
        lv_style_set_shadow_color(&style_slider_knob, COLOR_BTN_START);
        lv_style_set_bg_color(&style_round_btn_disabled, COLOR_DISABLED);
        lv_style_set_bg_grad_color(&style_round_btn_disabled, COLOR_DISABLED);
        lv_style_set_shadow_opa(&style_round_btn_disabled, 0);
        lv_style_set_outline_opa(&style_round_btn_disabled, 60);
        lv_style_set_opa(&style_round_btn_disabled, 160);

        style_inited = true;
    }

    /*----------------
     * Subjects
     *----------------*/
    lv_subject_init_int(&subject_wifi_level, 3);
    lv_subject_set_min_value_int(&subject_wifi_level, 0);
    lv_subject_set_max_value_int(&subject_wifi_level, 3);
    lv_subject_init_int(&subject_battery, 78);
    lv_subject_set_min_value_int(&subject_battery, 0);
    lv_subject_set_max_value_int(&subject_battery, 100);
    static char subject_time_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_time_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&subject_time,
                           subject_time_buf,
                           subject_time_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "10:24"
                          );
    lv_subject_init_int(&subject_budgie_mode, 0);
    lv_subject_set_min_value_int(&subject_budgie_mode, 0);
    lv_subject_set_max_value_int(&subject_budgie_mode, 4);
    lv_subject_init_int(&subject_photo_taken, 0);
    lv_subject_set_min_value_int(&subject_photo_taken, 0);
    lv_subject_set_max_value_int(&subject_photo_taken, 1);
    lv_subject_init_int(&subject_camera_origin, 0);
    lv_subject_set_min_value_int(&subject_camera_origin, 0);
    lv_subject_set_max_value_int(&subject_camera_origin, 1);
    lv_subject_init_int(&subject_chat_has_photo, 0);
    lv_subject_set_min_value_int(&subject_chat_has_photo, 0);
    lv_subject_set_max_value_int(&subject_chat_has_photo, 1);
    lv_subject_init_int(&subject_volume, 70);
    lv_subject_set_min_value_int(&subject_volume, 0);
    lv_subject_set_max_value_int(&subject_volume, 100);
    lv_subject_init_int(&subject_chirps, 1);
    lv_subject_set_min_value_int(&subject_chirps, 0);
    lv_subject_set_max_value_int(&subject_chirps, 1);
    lv_subject_init_int(&subject_brightness, 80);
    lv_subject_set_min_value_int(&subject_brightness, 10);
    lv_subject_set_max_value_int(&subject_brightness, 100);
    lv_subject_init_int(&subject_screen_timeout, 1);
    lv_subject_set_min_value_int(&subject_screen_timeout, 0);
    lv_subject_set_max_value_int(&subject_screen_timeout, 3);
    lv_subject_init_int(&subject_clock_format, 1);
    lv_subject_set_min_value_int(&subject_clock_format, 0);
    lv_subject_set_max_value_int(&subject_clock_format, 1);
    lv_subject_init_int(&subject_kid_safe, 0);
    lv_subject_set_min_value_int(&subject_kid_safe, 0);
    lv_subject_set_max_value_int(&subject_kid_safe, 1);
    static char subject_wifi_name_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_wifi_name_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&subject_wifi_name,
                           subject_wifi_name_buf,
                           subject_wifi_name_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "HomeNet"
                          );
    static char subject_web_url_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_web_url_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&subject_web_url,
                           subject_web_url_buf,
                           subject_web_url_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "192.168.4.1"
                          );
    static char subject_app_version_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_app_version_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&subject_app_version,
                           subject_app_version_buf,
                           subject_app_version_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "1.0.0"
                          );
    lv_subject_init_int(&subject_show_support, 0);
    lv_subject_set_min_value_int(&subject_show_support, 0);
    lv_subject_set_max_value_int(&subject_show_support, 1);

    /*----------------
     * Translations
     *----------------*/

    #ifndef LV_EDITOR_PREVIEW
        lv_translation_add_static(translation_languages, translation_tags, translation_texts);
        lv_translation_set_language(translation_languages[0]);
    #endif

#if defined(LV_USE_XML) && LV_USE_XML
    /* Register widgets */

    /* Check all fonts / default if needed. This prevents fonts that are used in one target but
       defined in another from causing assertion failures during rendering of the Preview. */
    check_font(&font_title, "font_title");
    check_font(&font_status, "font_status");
    check_font(&font_body, "font_body");
    check_font(&font_bubble_title, "font_bubble_title");
    check_font(&font_bubble_text, "font_bubble_text");
    check_font(&font_key_digit, "font_key_digit");
    check_font(&font_row_title, "font_row_title");

    /* Register fonts */
    lv_xml_register_font(NULL, "font_title", font_title);
    lv_xml_register_font(NULL, "font_status", font_status);
    lv_xml_register_font(NULL, "font_body", font_body);
    lv_xml_register_font(NULL, "font_bubble_title", font_bubble_title);
    lv_xml_register_font(NULL, "font_bubble_text", font_bubble_text);
    lv_xml_register_font(NULL, "font_key_digit", font_key_digit);
    lv_xml_register_font(NULL, "font_row_title", font_row_title);

    /* Register subjects */
    lv_xml_register_subject(NULL, "subject_wifi_level", &subject_wifi_level);
    lv_xml_register_subject(NULL, "subject_battery", &subject_battery);
    lv_xml_register_subject(NULL, "subject_time", &subject_time);
    lv_xml_register_subject(NULL, "subject_budgie_mode", &subject_budgie_mode);
    lv_xml_register_subject(NULL, "subject_photo_taken", &subject_photo_taken);
    lv_xml_register_subject(NULL, "subject_camera_origin", &subject_camera_origin);
    lv_xml_register_subject(NULL, "subject_chat_has_photo", &subject_chat_has_photo);
    lv_xml_register_subject(NULL, "subject_volume", &subject_volume);
    lv_xml_register_subject(NULL, "subject_chirps", &subject_chirps);
    lv_xml_register_subject(NULL, "subject_brightness", &subject_brightness);
    lv_xml_register_subject(NULL, "subject_screen_timeout", &subject_screen_timeout);
    lv_xml_register_subject(NULL, "subject_clock_format", &subject_clock_format);
    lv_xml_register_subject(NULL, "subject_kid_safe", &subject_kid_safe);
    lv_xml_register_subject(NULL, "subject_wifi_name", &subject_wifi_name);
    lv_xml_register_subject(NULL, "subject_web_url", &subject_web_url);
    lv_xml_register_subject(NULL, "subject_app_version", &subject_app_version);
    lv_xml_register_subject(NULL, "subject_show_support", &subject_show_support);

    /* Register callbacks */
    lv_xml_register_event_cb(NULL, "chat_t9_key_cb", chat_t9_key_cb);
    lv_xml_register_event_cb(NULL, "chat_screen_load_cb", chat_screen_load_cb);
    lv_xml_register_event_cb(NULL, "chat_backspace_cb", chat_backspace_cb);
    lv_xml_register_event_cb(NULL, "chat_case_cb", chat_case_cb);
    lv_xml_register_event_cb(NULL, "chat_mode_cb", chat_mode_cb);
    lv_xml_register_event_cb(NULL, "chat_space_cb", chat_space_cb);
    lv_xml_register_event_cb(NULL, "chat_clear_cb", chat_clear_cb);
    lv_xml_register_event_cb(NULL, "chat_send_cb", chat_send_cb);
    lv_xml_register_event_cb(NULL, "home_clear_cb", home_clear_cb);
    lv_xml_register_event_cb(NULL, "home_mic_cb", home_mic_cb);
    lv_xml_register_event_cb(NULL, "settings_clear_history_cb", settings_clear_history_cb);
    lv_xml_register_event_cb(NULL, "settings_power_off_cb", settings_power_off_cb);
#endif

    /* Register all the global assets so that they won't be created again when globals.xml is parsed.
     * While running in the editor skip this step to update the preview when the XML changes */
#if defined(LV_USE_XML) && LV_USE_XML && !defined(LV_EDITOR_PREVIEW)
    /* Register images */
    lv_xml_register_image(NULL, "img_wifi_0", img_wifi_0);
    lv_xml_register_image(NULL, "img_wifi_1", img_wifi_1);
    lv_xml_register_image(NULL, "img_wifi_2", img_wifi_2);
    lv_xml_register_image(NULL, "img_wifi_3", img_wifi_3);
    lv_xml_register_image(NULL, "img_battery_frame", img_battery_frame);
    lv_xml_register_image(NULL, "img_budgie_idle", img_budgie_idle);
    lv_xml_register_image(NULL, "img_budgie_listening", img_budgie_listening);
    lv_xml_register_image(NULL, "img_budgie_thinking", img_budgie_thinking);
    lv_xml_register_image(NULL, "img_budgie_talking", img_budgie_talking);
    lv_xml_register_image(NULL, "img_budgie_avatar", img_budgie_avatar);
    lv_xml_register_image(NULL, "img_icon_mic", img_icon_mic);
    lv_xml_register_image(NULL, "img_icon_camera", img_icon_camera);
    lv_xml_register_image(NULL, "img_icon_keyboard", img_icon_keyboard);
    lv_xml_register_image(NULL, "img_icon_settings", img_icon_settings);
    lv_xml_register_image(NULL, "img_icon_back", img_icon_back);
    lv_xml_register_image(NULL, "img_icon_back_light", img_icon_back_light);
    lv_xml_register_image(NULL, "img_icon_photo", img_icon_photo);
    lv_xml_register_image(NULL, "img_icon_camera_small", img_icon_camera_small);
    lv_xml_register_image(NULL, "img_icon_close_small", img_icon_close_small);
    lv_xml_register_image(NULL, "img_icon_backspace", img_icon_backspace);
    lv_xml_register_image(NULL, "img_icon_close", img_icon_close);
    lv_xml_register_image(NULL, "img_icon_send", img_icon_send);
    lv_xml_register_image(NULL, "img_icon_send_large", img_icon_send_large);
#endif

#if !defined(LV_USE_XML) || LV_USE_XML == 0
    /*--------------------
     *  Permanent screens
     *-------------------*/
    /* If XML is enabled it's assumed that the permanent screens are created
     * manually from XML using lv_xml_create() */
    chat_create();
    home_create();
#endif
}

void budgie_ui_set_target(uint32_t target)
{
    budgie_ui_target = target;
}

uint32_t budgie_ui_get_target(void)
{
    return budgie_ui_target;
}

bool budgie_ui_check_target(uint32_t target)
{
    return (budgie_ui_target & target) ? true : false;
}

/* Callbacks */
#if defined(LV_EDITOR_PREVIEW)
void __attribute__((weak)) chat_t9_key_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("chat_t9_key_cb was called\n");
}
void __attribute__((weak)) chat_screen_load_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("chat_screen_load_cb was called\n");
}
void __attribute__((weak)) chat_backspace_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("chat_backspace_cb was called\n");
}
void __attribute__((weak)) chat_case_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("chat_case_cb was called\n");
}
void __attribute__((weak)) chat_mode_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("chat_mode_cb was called\n");
}
void __attribute__((weak)) chat_space_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("chat_space_cb was called\n");
}
void __attribute__((weak)) chat_clear_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("chat_clear_cb was called\n");
}
void __attribute__((weak)) chat_send_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("chat_send_cb was called\n");
}
void __attribute__((weak)) home_clear_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("home_clear_cb was called\n");
}
void __attribute__((weak)) home_mic_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("home_mic_cb was called\n");
}
void __attribute__((weak)) settings_clear_history_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("settings_clear_history_cb was called\n");
}
void __attribute__((weak)) settings_power_off_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("settings_power_off_cb was called\n");
}
#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void check_font(lv_font_t ** font, const char * name)
{
    if (!(*font)) {
        *font = (lv_font_t *)LV_FONT_DEFAULT;
        LV_LOG_WARN("font `%s` was not set. Using `LV_FONT_DEFAULT` instead", name);
    }
}