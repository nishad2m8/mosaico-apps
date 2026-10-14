// SPDX-License-Identifier: Apache-2.0

#include "chat_history.h"

#include <stdlib.h>
#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "chat_history";

typedef struct {
    uint32_t id;
    char *user;
    char *assistant;
} turn_t;

struct chat_history_t {
    SemaphoreHandle_t lock;
    turn_t *turns;          /* ring buffer */
    size_t max_turns;
    size_t head;            /* index of the oldest turn */
    size_t count;
    uint32_t next_id;
};

static char *dup_psram(const char *s)
{
    const size_t n = strlen(s) + 1;
    char *d = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    if (d == NULL) {
        d = malloc(n);
    }
    if (d != NULL) {
        memcpy(d, s, n);
    }
    return d;
}

static void free_turn(turn_t *t)
{
    free(t->user);
    free(t->assistant);
    t->user = NULL;
    t->assistant = NULL;
}

esp_err_t chat_history_create(size_t max_turns, chat_history_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(max_turns > 0 && ret_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "bad args");
    chat_history_handle_t h = calloc(1, sizeof(*h));
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    h->turns = calloc(max_turns, sizeof(turn_t));
    h->lock = xSemaphoreCreateMutex();
    if (h->turns == NULL || h->lock == NULL) {
        chat_history_delete(h);
        return ESP_ERR_NO_MEM;
    }
    h->max_turns = max_turns;
    *ret_handle = h;
    return ESP_OK;
}

void chat_history_delete(chat_history_handle_t h)
{
    if (h == NULL) {
        return;
    }
    if (h->turns != NULL) {
        for (size_t i = 0; i < h->max_turns; i++) {
            free_turn(&h->turns[i]);
        }
        free(h->turns);
    }
    if (h->lock != NULL) {
        vSemaphoreDelete(h->lock);
    }
    free(h);
}

esp_err_t chat_history_add(chat_history_handle_t h, const char *user, const char *assistant)
{
    ESP_RETURN_ON_FALSE(h != NULL && user != NULL && assistant != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "bad args");
    turn_t t = { .user = dup_psram(user), .assistant = dup_psram(assistant) };
    if (t.user == NULL || t.assistant == NULL) {
        free_turn(&t);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    t.id = h->next_id++;
    if (h->count == h->max_turns) {
        free_turn(&h->turns[h->head]);
        h->turns[h->head] = t;
        h->head = (h->head + 1) % h->max_turns;
    } else {
        h->turns[(h->head + h->count) % h->max_turns] = t;
        h->count++;
    }
    xSemaphoreGive(h->lock);
    return ESP_OK;
}

void chat_history_clear(chat_history_handle_t h)
{
    if (h == NULL) {
        return;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    for (size_t i = 0; i < h->max_turns; i++) {
        free_turn(&h->turns[i]);
    }
    h->head = 0;
    h->count = 0;
    xSemaphoreGive(h->lock);
}

size_t chat_history_snapshot(chat_history_handle_t h, size_t keep_turns,
                             ai_message_t *out, size_t out_max)
{
    if (h == NULL || out == NULL) {
        return 0;
    }
    size_t written = 0;
    xSemaphoreTake(h->lock, portMAX_DELAY);
    size_t turns = h->count < keep_turns ? h->count : keep_turns;
    if (turns * 2 > out_max) {
        turns = out_max / 2;
    }
    for (size_t i = h->count - turns; i < h->count; i++) {
        const turn_t *t = &h->turns[(h->head + i) % h->max_turns];
        char *u = dup_psram(t->user);
        char *a = dup_psram(t->assistant);
        if (u == NULL || a == NULL) {
            free(u);
            free(a);
            break;
        }
        out[written++] = (ai_message_t) { .role = AI_ROLE_USER, .content = u };
        out[written++] = (ai_message_t) { .role = AI_ROLE_ASSISTANT, .content = a };
    }
    xSemaphoreGive(h->lock);
    return written;
}

void chat_history_release(ai_message_t *msgs, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free((void *)msgs[i].content);
        msgs[i].content = NULL;
    }
}

size_t chat_history_list(chat_history_handle_t h, chat_history_entry_t *out, size_t out_max)
{
    if (h == NULL || out == NULL) {
        return 0;
    }
    size_t written = 0;
    xSemaphoreTake(h->lock, portMAX_DELAY);
    for (size_t i = h->count; i > 0 && written < out_max; i--) {
        const turn_t *t = &h->turns[(h->head + i - 1) % h->max_turns];
        chat_history_entry_t e = { .id = t->id, .user = dup_psram(t->user), .assistant = dup_psram(t->assistant) };
        if (e.user == NULL || e.assistant == NULL) {
            free(e.user);
            free(e.assistant);
            break;
        }
        out[written++] = e;
    }
    xSemaphoreGive(h->lock);
    return written;
}

void chat_history_list_release(chat_history_entry_t *entries, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free(entries[i].user);
        free(entries[i].assistant);
        entries[i].user = NULL;
        entries[i].assistant = NULL;
    }
}

esp_err_t chat_history_remove(chat_history_handle_t h, uint32_t id)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    esp_err_t err = ESP_ERR_NOT_FOUND;
    xSemaphoreTake(h->lock, portMAX_DELAY);
    for (size_t i = 0; i < h->count; i++) {
        if (h->turns[(h->head + i) % h->max_turns].id != id) {
            continue;
        }
        free_turn(&h->turns[(h->head + i) % h->max_turns]);
        /* Close the gap so the ring stays oldest..newest without holes */
        for (size_t j = i; j + 1 < h->count; j++) {
            h->turns[(h->head + j) % h->max_turns] = h->turns[(h->head + j + 1) % h->max_turns];
        }
        h->turns[(h->head + h->count - 1) % h->max_turns] = (turn_t) { 0 };
        h->count--;
        err = ESP_OK;
        break;
    }
    xSemaphoreGive(h->lock);
    return err;
}
