/**
 * @file budgie_chat.h
 * Chat screen logic: T9 multi-tap typing and the hooks to connect the assistant.
 */

#ifndef BUDGIE_CHAT_H
#define BUDGIE_CHAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* The simulator build defines LV_LVGL_H_INCLUDE_SIMPLE; the Editor preview only has lvgl/ on the include path */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/* Show `text` in the chat screen's response area (the chat screen is permanent, so this
 * works from any screen). Call with the LVGL lock held. */
void budgie_chat_set_response(const char * text);

/* Called when the user taps Send. `text` is the typed message (may be "" when only a photo is sent),
 * `with_photo` is true when a photo from the camera is attached. The default implementation in
 * budgie_chat.c only echoes the message; define BUDGIE_CHAT_EXTERNAL_SEND and implement this
 * function yourself to pass the message (and the captured photo) to the assistant, then call
 * budgie_chat_set_response(). */
void budgie_chat_on_send(const char * text, bool with_photo);

/* Called when Settings > Chat history > Clear is tapped, so the assistant can forget the
 * conversation too. Implemented by the app when BUDGIE_CHAT_EXTERNAL_SEND is defined. */
void budgie_chat_on_history_cleared(void);

/* Event callbacks referenced from screens/chat.xml and components/t9_key.xml */
void chat_t9_key_cb(lv_event_t * e);
void chat_backspace_cb(lv_event_t * e);
void chat_case_cb(lv_event_t * e);
void chat_mode_cb(lv_event_t * e);
void chat_space_cb(lv_event_t * e);
void chat_clear_cb(lv_event_t * e);
void chat_send_cb(lv_event_t * e);
void chat_screen_load_cb(lv_event_t * e);

/* Event callback referenced from screens/settings.xml */
void settings_clear_history_cb(lv_event_t * e);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*BUDGIE_CHAT_H*/
