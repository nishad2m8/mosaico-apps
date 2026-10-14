// SPDX-License-Identifier: Apache-2.0
//
// Bounded conversation memory: the last N user/assistant exchanges, stored in
// PSRAM. Thread-safe (the UI clears it while the assistant task appends).
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ai_client.h"
#include "esp_err.h"

typedef struct chat_history_t *chat_history_handle_t;

esp_err_t chat_history_create(size_t max_turns, chat_history_handle_t *ret_handle);
void chat_history_delete(chat_history_handle_t handle);

/** Append one exchange, dropping the oldest when full. */
esp_err_t chat_history_add(chat_history_handle_t handle, const char *user, const char *assistant);

void chat_history_clear(chat_history_handle_t handle);

/**
 * Copy up to @p keep_turns most recent exchanges into @p out as messages
 * (user, assistant, ...). The strings are heap copies so they stay valid after
 * the lock is released; free them with chat_history_release().
 * Returns the number of messages written.
 */
size_t chat_history_snapshot(chat_history_handle_t handle, size_t keep_turns,
                             ai_message_t *out, size_t out_max);
void chat_history_release(ai_message_t *msgs, size_t count);

/* One exchange for display; `id` stays the same while the exchange is kept */
typedef struct {
    uint32_t id;
    char *user;
    char *assistant;
} chat_history_entry_t;

/**
 * Copy up to @p out_max exchanges, newest first, as heap strings that stay
 * valid after the lock is released; free them with chat_history_list_release().
 * Returns the number of entries written.
 */
size_t chat_history_list(chat_history_handle_t handle, chat_history_entry_t *out, size_t out_max);
void chat_history_list_release(chat_history_entry_t *entries, size_t count);

/** Forget one exchange. ESP_ERR_NOT_FOUND when it is no longer kept. */
esp_err_t chat_history_remove(chat_history_handle_t handle, uint32_t id);
