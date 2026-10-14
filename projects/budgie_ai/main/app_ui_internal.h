// SPDX-License-Identifier: Apache-2.0
//
// Private interface between app_ui.c and app_ui_camera.c. Functions without
// a context note run on the LVGL task only.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "app_ui.h"
#include "camera_io.h"
#include "lvgl.h"

/* Camera result handed from the camera task to the LVGL task; heap fields are
 * owned by the message until ui_camera_handle() takes or frees them. */
typedef struct {
    camera_io_event_type_t type;
    uint8_t *jpeg;
    size_t jpeg_len;
    uint16_t *thumb;             /* UI_CAMERA_THUMB_SIZE^2 RGB565 */
} ui_camera_msg_t;

#define UI_CAMERA_THUMB_SIZE 44

/* app_ui.c */
void app_ui_internal_post_camera(ui_camera_msg_t *msg);   /* any task; takes ownership */
void app_ui_internal_toast(const char *text);
const app_ui_callbacks_t *app_ui_internal_callbacks(void);

/* app_ui_reply.c: reply bubble scroll and full-reply popup on `home` */
void ui_reply_init(void);
void ui_reply_set(const char *title, const char *text);   /* NULL keeps the current value */
bool ui_reply_popup_open(void);
/** Full-text popup on `screen` (closed by its x or the backdrop); replaces an open one. */
void ui_reply_show_popup(lv_obj_t *screen, const char *title, const char *text);

/* app_ui_history.c: History screen, opened by tapping the idle bubble */
void ui_history_init(void);

/* app_ui_camera.c */
void ui_camera_init(void);
void ui_camera_poll(void);                    /* detects entering/leaving the camera screen */
void ui_camera_handle(ui_camera_msg_t *msg);  /* frees the message payload */
/** Photo attached in chat, or NULL. Valid until the attachment is removed. */
const uint8_t *ui_camera_attached_photo(size_t *len);
