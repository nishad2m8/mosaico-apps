// SPDX-License-Identifier: Apache-2.0
//
// Bridge between the application and the Budgie LVGL UI (ui/budgie_ui).
//
// app_ui owns LVGL: it starts the display, builds the screens and runs all UI
// updates on the LVGL task. Other tasks never call LVGL; they post messages
// with app_ui_post_*(), which are thread-safe and non-blocking. Messages are
// applied by an LVGL timer, so they may arrive a few milliseconds later.
//
// UI callbacks (app_ui_callbacks_t) run on the LVGL task: keep them short.
#pragma once

#include <stdbool.h>
#include "assistant.h"
#include "camera_io.h"
#include "esp_err.h"
#include "wifi_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*on_mic)(void *user_ctx);                               /* home mic / Send tapped */
    void (*on_clear_recording)(void *user_ctx);                   /* home red Clear tapped */
    /* chat Send (text and/or attached photo, shown as text only) and camera Send from home */
    esp_err_t (*on_ask)(const assistant_question_t *question, void *user_ctx);
    void (*on_clear_history)(void *user_ctx);                     /* Settings > Clear, History > Clear all */
    /* History screen: newest-first copy (release with chat_history_list_release) and delete one */
    size_t (*list_history)(chat_history_entry_t *out, size_t out_max, void *user_ctx);
    void (*on_delete_history)(uint32_t id, void *user_ctx);
    void (*on_power_off)(void *user_ctx);                         /* Settings > Power off */
    void *user_ctx;
} app_ui_callbacks_t;

/** Start the display + LVGL, create the screens and show home. */
esp_err_t app_ui_start(const app_ui_callbacks_t *callbacks, const char *app_version);

void app_ui_post_assistant_event(const assistant_event_t *event);
void app_ui_post_wifi(const wifi_mgr_status_t *status);
void app_ui_post_battery(int percent);
/** Settings changed elsewhere (web page): refresh controls and brightness. */
void app_ui_post_config_changed(void);
/** Short message shown as a toast for a few seconds. */
void app_ui_post_notice(const char *text);
/** Wake the screen if it is dimmed. */
void app_ui_post_wake(void);
/** Wake and return to the home screen. */
void app_ui_post_go_home(void);

/** Give the UI the camera used by the camera screen. Call after app_ui_start(). */
void app_ui_attach_camera(camera_io_handle_t camera);

/** camera_io event callback (runs on the camera task). */
void app_ui_camera_event_cb(const camera_io_event_t *event, void *user_ctx);

/** Turn the panel dark right away (power-off path). Any task. */
void app_ui_blank_display(void);

#ifdef __cplusplus
}
#endif
