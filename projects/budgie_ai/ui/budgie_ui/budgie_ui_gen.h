/**
 * @file budgie_ui_gen.h
 */

#ifndef LVGL_PRO_BUDGIE_UI_GEN_H
#define LVGL_PRO_BUDGIE_UI_GEN_H

#ifndef UI_SUBJECT_STRING_LENGTH
#define UI_SUBJECT_STRING_LENGTH 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
    #include "lvgl_private.h"
#else
    #include "lvgl/lvgl.h"
    #include "lvgl/lvgl_private.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif



/* Prototypes for target functions, needed by responsive const definitions */

void budgie_ui_set_target(uint32_t target);
uint32_t budgie_ui_get_target(void);
bool budgie_ui_check_target(uint32_t target);

/*********************
 *      DEFINES
 *********************/

#define BUDGIE_UI_TARGET_UNDEFINED  (0 << 1)
#define BUDGIE_UI_TARGET_TARGET1    (1 << 1)
#define BUDGIE_UI_TARGET_ALL        0x0FFFFFFF

/* By default compile for all targets, allowing to switch to any targets at runtime */
#ifndef BUDGIE_UI_COMPILE_TARGET
#define BUDGIE_UI_COMPILE_TARGET BUDGIE_UI_TARGET_ALL
#endif

#define BUDGIE_UI_CHECK_COMPILE_TARGET(target) (BUDGIE_UI_COMPILE_TARGET & (target) ? 1 : 0)

/**
 * Screen background
 */
#define COLOR_BG lv_color_hex(0xf3f5fa)
/**
 * Primary text (brand title)
 */
#define COLOR_TEXT lv_color_hex(0x0f1f3d)
/**
 * Status icons and status text
 */
#define COLOR_ICON lv_color_hex(0x4a5578)
/**
 * Secondary text
 */
#define COLOR_MUTED lv_color_hex(0x6b7590)
/**
 * Thin separators
 */
#define COLOR_DIVIDER lv_color_hex(0xd5dae5)
/**
 * Inactive wifi bars
 */
#define COLOR_ICON_DIM lv_color_hex(0xccd2de)
/**
 * Battery fill when low
 */
#define COLOR_BATTERY_LOW lv_color_hex(0xe5484d)
/**
 * Round action button gradient, left (blue)
 */
#define COLOR_BTN_START lv_color_hex(0x5b7ff5)
/**
 * Round action button gradient, right (teal)
 */
#define COLOR_BTN_END lv_color_hex(0x5fd0c2)
/**
 * Soft ring around the round action buttons
 */
#define COLOR_BTN_HALO lv_color_hex(0xdde6fb)
/**
 * Speech bubble fill
 */
#define COLOR_BUBBLE_BG lv_color_hex(0xf8faff)
/**
 * Speech bubble outline
 */
#define COLOR_BUBBLE_BORDER lv_color_hex(0xc9d6f2)
/**
 * Cards, keys and input fields
 */
#define COLOR_CARD lv_color_hex(0xffffff)
/**
 * Keypad key while pressed
 */
#define COLOR_KEY_PRESSED lv_color_hex(0xe6eaf2)
/**
 * Keypad function key background
 */
#define COLOR_KEY_FN lv_color_hex(0xeaeff9)
/**
 * Camera screen background
 */
#define COLOR_CAM_BG lv_color_hex(0x0e1424)
/**
 * Camera screen round buttons
 */
#define COLOR_CAM_OVERLAY lv_color_hex(0x1c2438)
/**
 * Disabled button fill
 */
#define COLOR_DISABLED lv_color_hex(0x8a93ab)
/**
 * Slider / switch / segmented control track
 */
#define COLOR_TRACK lv_color_hex(0xe3e8f3)
/**
 * Destructive actions (clear history)
 */
#define COLOR_DANGER lv_color_hex(0xd64545)
/**
 * Soft ring around the red Clear button
 */
#define COLOR_DANGER_HALO lv_color_hex(0xf7dada)
/**
 * At or below this % the battery turns red
 */
#define BATTERY_LOW_LEVEL 20
#define MODE_IDLE 0
#define MODE_LISTENING 1
#define MODE_THINKING 2
#define MODE_TALKING 3
/**
 * Recording stopped and held: mic shows Send, red Clear appears
 */
#define MODE_RECORDED 4
#define ORIGIN_HOME 0
#define ORIGIN_CHAT 1


#ifndef LV_XML_EVAL_STRING_BUF_SIZE
    #define LV_XML_EVAL_STRING_BUF_SIZE 256
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL VARIABLES
 **********************/

/*-------------------
 * Permanent screens
 *------------------*/

extern lv_obj_t * chat;
extern lv_obj_t * home;

/*----------------
 * Global styles
 *----------------*/

extern lv_style_t style_status_text;
extern lv_style_t style_round_btn;
extern lv_style_t style_round_btn_pressed;
extern lv_style_t style_key;
extern lv_style_t style_key_pressed;
extern lv_style_t style_key_fn;
extern lv_style_t style_settings_card;
extern lv_style_t style_section_title;
extern lv_style_t style_row_divider;
extern lv_style_t style_switch_track;
extern lv_style_t style_switch_on;
extern lv_style_t style_switch_knob;
extern lv_style_t style_slider_track;
extern lv_style_t style_slider_fill;
extern lv_style_t style_slider_knob;
extern lv_style_t style_round_btn_disabled;

/*----------------
 * Fonts
 *----------------*/

/* Targets: any */
extern lv_font_t * font_title;
extern lv_font_t * font_status;
extern lv_font_t * font_body;
extern lv_font_t * font_bubble_title;
extern lv_font_t * font_bubble_text;
extern lv_font_t * font_key_digit;
extern lv_font_t * font_row_title;


/*----------------
 * Images
 *----------------*/

/* Targets: any */
extern const void * img_wifi_0;
extern const void * img_wifi_1;
extern const void * img_wifi_2;
extern const void * img_wifi_3;
extern const void * img_battery_frame;
extern const void * img_budgie_idle;
extern const void * img_budgie_listening;
extern const void * img_budgie_thinking;
extern const void * img_budgie_talking;
extern const void * img_budgie_avatar;
extern const void * img_icon_mic;
extern const void * img_icon_camera;
extern const void * img_icon_keyboard;
extern const void * img_icon_settings;
extern const void * img_icon_back;
extern const void * img_icon_back_light;
extern const void * img_icon_photo;
extern const void * img_icon_camera_small;
extern const void * img_icon_close_small;
extern const void * img_icon_backspace;
extern const void * img_icon_close;
extern const void * img_icon_send;
extern const void * img_icon_send_large;

/*----------------
 * Subjects
 *----------------*/

extern lv_subject_t subject_wifi_level;
extern lv_subject_t subject_battery;
extern lv_subject_t subject_time;
extern lv_subject_t subject_budgie_mode;
extern lv_subject_t subject_photo_taken;
extern lv_subject_t subject_camera_origin;
extern lv_subject_t subject_chat_has_photo;
extern lv_subject_t subject_volume;
extern lv_subject_t subject_chirps;
extern lv_subject_t subject_brightness;
extern lv_subject_t subject_screen_timeout;
extern lv_subject_t subject_clock_format;
extern lv_subject_t subject_kid_safe;
extern lv_subject_t subject_wifi_name;
extern lv_subject_t subject_web_url;
extern lv_subject_t subject_app_version;
extern lv_subject_t subject_show_support;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*----------------
 * Event Callbacks
 *----------------*/

void chat_t9_key_cb(lv_event_t * e);
void chat_screen_load_cb(lv_event_t * e);
void chat_backspace_cb(lv_event_t * e);
void chat_case_cb(lv_event_t * e);
void chat_mode_cb(lv_event_t * e);
void chat_space_cb(lv_event_t * e);
void chat_clear_cb(lv_event_t * e);
void chat_send_cb(lv_event_t * e);
void home_clear_cb(lv_event_t * e);
void home_mic_cb(lv_event_t * e);
void settings_clear_history_cb(lv_event_t * e);
void settings_power_off_cb(lv_event_t * e);

/**
 * Initialize the component library
 */

void budgie_ui_init_gen(const char * asset_path);

/**********************
 *      MACROS
 **********************/

/**********************
 *   POST INCLUDES
 **********************/

/*Include all the widgets, components and screens of this library*/
#include "components/settings_choice_gen.h"
#include "components/settings_option_gen.h"
#include "components/settings_row_gen.h"
#include "components/settings_slider_gen.h"
#include "components/speech_bubble_gen.h"
#include "components/status_bar_gen.h"
#include "components/t9_key_gen.h"
#include "screens/camera_gen.h"
#include "screens/chat_gen.h"
#include "screens/home_gen.h"
#include "screens/settings_gen.h"

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LVGL_PRO_BUDGIE_UI_GEN_H*/