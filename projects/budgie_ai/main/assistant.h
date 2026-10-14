// SPDX-License-Identifier: Apache-2.0
//
// Voice/text assistant pipeline:
//   voice: record -> speech-to-text -> chat -> text-to-speech
//   text:  chat -> text-to-speech
// Requests run one at a time on the assistant's worker task. A new recording
// may interrupt a spoken reply (barge-in) but not a request in progress.
//
// Events are delivered from the worker task (or from the caller's task for
// LISTENING); the receiver must hop to its own context before touching LVGL.
#pragma once

#include <stdbool.h>
#include "audio_io.h"
#include "chat_history.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct assistant_t *assistant_handle_t;

/* Same values as the UI's subject_budgie_mode (mode_* consts in globals.xml) */
typedef enum {
    ASSISTANT_MODE_IDLE = 0,
    ASSISTANT_MODE_LISTENING = 1,
    ASSISTANT_MODE_THINKING = 2,
    ASSISTANT_MODE_TALKING = 3,
    ASSISTANT_MODE_RECORDED = 4,   /* recording stopped and held until sent or cleared */
} assistant_mode_t;

typedef enum {
    ASSISTANT_EVT_MODE,          /* mode changed; text unused */
    ASSISTANT_EVT_USER_TEXT,     /* what the user said (transcript or typed) */
    ASSISTANT_EVT_REPLY,         /* assistant reply text */
    ASSISTANT_EVT_ERROR,         /* human-readable failure; mode returns to IDLE */
    ASSISTANT_EVT_REPLY_DONE,    /* reply finished playing (or was not spoken); not sent on barge-in */
} assistant_event_type_t;

typedef struct {
    assistant_event_type_t type;
    assistant_mode_t mode;
    const char *text;            /* valid only during the callback */
} assistant_event_t;

typedef void (*assistant_event_cb_t)(const assistant_event_t *event, void *user_ctx);

typedef struct {
    audio_io_handle_t audio;     /* borrowed; must outlive the assistant */
    assistant_event_cb_t event_cb;
    void *user_ctx;
} assistant_config_t;

esp_err_t assistant_create(const assistant_config_t *config, assistant_handle_t *ret_handle);

/** Start recording (stops a spoken reply). ESP_ERR_INVALID_STATE while busy. */
esp_err_t assistant_listen_start(assistant_handle_t handle);

/** Stop recording and process the speech. */
esp_err_t assistant_listen_stop(assistant_handle_t handle);

/** Toggle: start listening, or stop and send if already listening. */
esp_err_t assistant_listen_toggle(assistant_handle_t handle);

/**
 * Stop recording and hold the speech (mode RECORDED) until
 * assistant_send_recording() or assistant_listen_cancel(). A too-short
 * recording returns ESP_ERR_INVALID_SIZE and goes back to IDLE.
 */
esp_err_t assistant_listen_hold(assistant_handle_t handle);

/** Send the held recording. ESP_ERR_INVALID_STATE when nothing is held. */
esp_err_t assistant_send_recording(assistant_handle_t handle);

/** Stop recording, or drop a held recording, without sending. */
void assistant_listen_cancel(assistant_handle_t handle);

/* A typed and/or photo question */
typedef struct {
    const char *text;            /* may be NULL or "" when a photo is attached */
    const uint8_t *jpeg;         /* optional photo, copied by assistant_ask() */
    size_t jpeg_len;
    bool speak;                  /* also play the reply through the speaker */
} assistant_question_t;

/** Ask a question. ESP_ERR_INVALID_STATE while busy. */
esp_err_t assistant_ask(assistant_handle_t handle, const assistant_question_t *question);

/** Ask a typed question and speak the reply (same as assistant_ask with speak). */
esp_err_t assistant_send_text(assistant_handle_t handle, const char *text);

/** Stop speaking the current reply (the text stays on screen). */
void assistant_stop_speaking(assistant_handle_t handle);

void assistant_clear_history(assistant_handle_t handle);

/** Newest-first copy of the conversation; see chat_history_list(). Any task. */
size_t assistant_history_list(assistant_handle_t handle, chat_history_entry_t *out, size_t out_max);

/** Forget one exchange by its chat_history_entry_t id. Any task. */
esp_err_t assistant_history_remove(assistant_handle_t handle, uint32_t id);

bool assistant_is_listening(assistant_handle_t handle);

/** True while a stopped recording is held (mode RECORDED). */
bool assistant_has_recording(assistant_handle_t handle);

#ifdef __cplusplus
}
#endif
