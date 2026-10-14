// SPDX-License-Identifier: Apache-2.0
//
// State is guarded by `lock`:
//   listening  recording in progress (set/cleared by callers)
//   recorded   a stopped recording is held for assistant_send_recording()
//   pending    jobs queued or running on the worker task (busy while > 0)
//   speaking   the worker is playing the reply (barge-in allowed)
// Buffers used by a job (config copy, prompt, transcript, reply, messages)
// are owned by the instance and touched only by the worker task.

#include "assistant.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ai_client.h"
#include "app_config.h"
#include "chat_history.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "assistant";

#define WORKER_STACK           (10 * 1024)
#define WORKER_PRIO            5
#define JOB_QUEUE_LEN          1
#define REPLY_MAX              4096
#define TRANSCRIPT_MAX         1024
#define PROMPT_MAX             (APP_CONFIG_PROMPT_LEN + 256)
#define MESSAGES_MAX           (2 + 2 * APP_CONFIG_HISTORY_TURNS_MAX)
#define MIN_SPEECH_MS          400
#define TEXT_INPUT_MAX         1024
#define VOICE_LIST_MARKER      "voices: ["
#define PHOTO_QUESTION         "What is in this photo? Describe it briefly."
#define PHOTO_HISTORY_NOTE     " [photo]"

#define KID_SAFE_PROMPT \
    " The user is a child: keep answers simple, kind, encouraging and family-friendly."
#define ASCII_PROMPT \
    " Use only plain ASCII characters: no emoji, no markdown, no special quotes."

typedef enum {
    JOB_TEXT,
    JOB_VOICE,
} job_type_t;

typedef struct {
    job_type_t type;
    bool speak;                  /* play the reply through the speaker */
    char *text;                  /* JOB_TEXT: heap string owned by the job */
    uint8_t *jpeg;               /* JOB_TEXT: optional photo owned by the job */
    size_t jpeg_len;
    const int16_t *pcm;          /* JOB_VOICE: audio_io buffer, valid until the next recording */
    size_t samples;
} job_t;

struct assistant_t {
    assistant_config_t config;
    SemaphoreHandle_t lock;
    QueueHandle_t jobs;
    TaskHandle_t worker;
    ai_client_handle_t ai;
    chat_history_handle_t history;

    bool listening;
    bool recorded;
    const int16_t *recorded_pcm;   /* audio_io buffer, valid until the next recording */
    size_t recorded_samples;
    uint8_t pending;
    volatile bool speaking;
    volatile bool stop_speaking;

    /* Worker-owned buffers */
    app_config_t *cfg;
    char *prompt;
    char *transcript;
    char *reply;
    ai_message_t messages[MESSAGES_MAX];
};

static void *alloc_psram(size_t size)
{
    void *p = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM);
    return p != NULL ? p : calloc(1, size);
}

static void emit(assistant_handle_t h, assistant_event_type_t type, assistant_mode_t mode,
                 const char *text)
{
    if (h->config.event_cb != NULL) {
        const assistant_event_t evt = { .type = type, .mode = mode, .text = text };
        h->config.event_cb(&evt, h->config.user_ctx);
    }
}

static void finish_job(assistant_handle_t h)
{
    xSemaphoreTake(h->lock, portMAX_DELAY);
    if (h->pending > 0) {
        h->pending--;
    }
    h->speaking = false;
    xSemaphoreGive(h->lock);
}

static void fail(assistant_handle_t h, const char *message)
{
    ESP_LOGW(TAG, "%s", message);
    emit(h, ASSISTANT_EVT_ERROR, ASSISTANT_MODE_IDLE, message);
}

/* ---------- Worker ---------- */

static const char *pick(const char *preferred, const char *fallback)
{
    return (preferred != NULL && preferred[0] != '\0') ? preferred : fallback;
}

static ai_endpoint_t speech_endpoint(const app_config_t *cfg, const char *model)
{
    return (ai_endpoint_t) {
        .base_url = pick(cfg->speech_base_url, cfg->api_base_url),
        .api_key = pick(cfg->speech_api_key, cfg->api_key),
        .model = model,
    };
}

static size_t build_messages(assistant_handle_t h, const char *user_text,
                             const uint8_t *jpeg, size_t jpeg_len)
{
    const app_config_t *cfg = h->cfg;
    snprintf(h->prompt, PROMPT_MAX, "%s%s%s", cfg->system_prompt,
             cfg->kid_safe ? KID_SAFE_PROMPT : "", ASCII_PROMPT);

    size_t n = 0;
    h->messages[n++] = (ai_message_t) { .role = AI_ROLE_SYSTEM, .content = h->prompt };
    n += chat_history_snapshot(h->history, cfg->history_turns, &h->messages[n], MESSAGES_MAX - 2);
    h->messages[n++] = (ai_message_t) {
        .role = AI_ROLE_USER, .content = user_text, .image_jpeg = jpeg, .image_len = jpeg_len,
    };
    return n;
}

static esp_err_t speech_sink(const int16_t *pcm, size_t samples, uint32_t rate, void *ctx)
{
    assistant_handle_t h = ctx;
    if (h->stop_speaking) {
        return ESP_ERR_INVALID_STATE;
    }
    return audio_io_play(h->config.audio, pcm, samples, rate);
}

/* Providers such as Groq reject unknown voices with
 * "voice must be one of the following voices: [autumn diana ...]". */
static bool suggested_voice(const char *error, char *out, size_t out_size)
{
    const char *p = strstr(error, VOICE_LIST_MARKER);
    if (p == NULL) {
        return false;
    }
    p += strlen(VOICE_LIST_MARKER);
    const size_t n = strcspn(p, " ,]\"'");
    if (n == 0 || n >= out_size) {
        return false;
    }
    memcpy(out, p, n);
    out[n] = '\0';
    return true;
}

/* Persist the voice so the web page shows it and later replies use it */
static void remember_voice(const char *voice)
{
    app_config_t *cfg = app_config_alloc();
    if (cfg != NULL) {
        app_config_get(cfg);
        strlcpy(cfg->tts_voice, voice, sizeof(cfg->tts_voice));
        app_config_set(cfg);
        free(cfg);
    }
}

static void speak_reply(assistant_handle_t h)
{
    app_config_t *cfg = h->cfg;
    if (!cfg->tts_enabled || cfg->tts_model[0] == '\0') {
        return;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    h->speaking = true;
    h->stop_speaking = false;
    xSemaphoreGive(h->lock);

    if (cfg->chirps) {
        audio_io_chirp(h->config.audio);
    }
    const ai_endpoint_t ep = speech_endpoint(cfg, cfg->tts_model);
    esp_err_t err = ai_client_speech(h->ai, &ep, cfg->tts_voice, h->reply, speech_sink, h);
    char voice[APP_CONFIG_VOICE_LEN];
    if (err != ESP_OK && !h->stop_speaking &&
            suggested_voice(ai_client_last_error(h->ai), voice, sizeof(voice))) {
        ESP_LOGW(TAG, "voice \"%s\" not offered by this provider; using \"%s\"", cfg->tts_voice, voice);
        strlcpy(cfg->tts_voice, voice, sizeof(cfg->tts_voice));
        remember_voice(voice);
        err = ai_client_speech(h->ai, &ep, cfg->tts_voice, h->reply, speech_sink, h);
    }
    audio_io_play_end(h->config.audio);
    if (err != ESP_OK && !h->stop_speaking && err != ESP_ERR_INVALID_STATE) {
        /* The reply is already on screen; a voice failure is not fatal */
        ESP_LOGW(TAG, "speech failed: %s", ai_client_last_error(h->ai));
    }
}

static void run_job(assistant_handle_t h, job_t *job)
{
    app_config_get(h->cfg);
    const app_config_t *cfg = h->cfg;
    if (cfg->api_key[0] == '\0' && strncmp(cfg->api_base_url, "http://", 7) != 0) {
        fail(h, "Set up my AI key first: open the web setup page shown in Settings.");
        return;
    }
    emit(h, ASSISTANT_EVT_MODE, ASSISTANT_MODE_THINKING, NULL);

    if (job->type == JOB_VOICE) {
        const ai_endpoint_t ep = speech_endpoint(cfg, cfg->stt_model);
        if (ai_client_transcribe(h->ai, &ep, cfg->stt_language, job->pcm, job->samples,
                                 audio_io_record_rate(h->config.audio), h->transcript, TRANSCRIPT_MAX) != ESP_OK) {
            fail(h, ai_client_last_error(h->ai));
            return;
        }
        if (h->transcript[strspn(h->transcript, " \t\r\n.")] == '\0') {
            fail(h, "Sorry, I didn't catch that. Try again?");
            return;
        }
    } else if (job->text != NULL && job->text[0] != '\0') {
        strlcpy(h->transcript, job->text, TRANSCRIPT_MAX);
    } else {
        strlcpy(h->transcript, PHOTO_QUESTION, TRANSCRIPT_MAX);
    }
    emit(h, ASSISTANT_EVT_USER_TEXT, ASSISTANT_MODE_THINKING, h->transcript);

    const bool photo = job->jpeg != NULL;
    const size_t count = build_messages(h, h->transcript, job->jpeg, job->jpeg_len);
    const ai_chat_params_t params = {
        .endpoint = {
            .base_url = cfg->api_base_url,
            .api_key = cfg->api_key,
            .model = photo ? pick(cfg->vision_model, cfg->chat_model) : cfg->chat_model,
        },
        .max_tokens = cfg->max_tokens,
        .temperature = cfg->temperature_x10 / 10.0f,
    };
    esp_err_t err = ai_client_chat(h->ai, &params, h->messages, count, h->reply, REPLY_MAX);
    /* Release the history copies (between the system prompt and the user message) */
    chat_history_release(&h->messages[1], count - 2);
    if (err != ESP_OK) {
        fail(h, ai_client_last_error(h->ai));
        return;
    }

    /* The photo itself is not kept: later turns only see that one was shown */
    if (photo) {
        strlcat(h->transcript, PHOTO_HISTORY_NOTE, TRANSCRIPT_MAX);
    }
    chat_history_add(h->history, h->transcript, h->reply);
    emit(h, ASSISTANT_EVT_REPLY, ASSISTANT_MODE_TALKING, h->reply);
    emit(h, ASSISTANT_EVT_MODE, ASSISTANT_MODE_TALKING, NULL);
    if (job->speak) {
        speak_reply(h);
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    const bool barged_in = h->listening;   /* a new recording took over */
    xSemaphoreGive(h->lock);
    if (!barged_in) {
        emit(h, ASSISTANT_EVT_REPLY_DONE, ASSISTANT_MODE_TALKING, NULL);
    }
}

static void worker_task(void *arg)
{
    assistant_handle_t h = arg;
    job_t job;
    while (true) {
        if (xQueueReceive(h->jobs, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        run_job(h, &job);
        free(job.text);
        free(job.jpeg);
        finish_job(h);
    }
}

/* ---------- Public API ---------- */

esp_err_t assistant_create(const assistant_config_t *config, assistant_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config != NULL && config->audio != NULL && ret_handle != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "bad args");
    assistant_handle_t h = calloc(1, sizeof(*h));
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    h->config = *config;

    esp_err_t err = ESP_ERR_NO_MEM;
    const ai_client_config_t ai_cfg = AI_CLIENT_DEFAULT_CONFIG();
    h->lock = xSemaphoreCreateMutex();
    h->jobs = xQueueCreate(JOB_QUEUE_LEN, sizeof(job_t));
    h->cfg = app_config_alloc();
    h->prompt = alloc_psram(PROMPT_MAX);
    h->transcript = alloc_psram(TRANSCRIPT_MAX);
    h->reply = alloc_psram(REPLY_MAX);
    if (h->lock == NULL || h->jobs == NULL || h->cfg == NULL || h->prompt == NULL ||
            h->transcript == NULL || h->reply == NULL) {
        goto fail;
    }
    if ((err = ai_client_create(&ai_cfg, &h->ai)) != ESP_OK ||
            (err = chat_history_create(APP_CONFIG_HISTORY_TURNS_MAX, &h->history)) != ESP_OK) {
        goto fail;
    }
    if (xTaskCreate(worker_task, "assistant", WORKER_STACK, h, WORKER_PRIO, &h->worker) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    *ret_handle = h;
    return ESP_OK;

fail:
    /* No task exists yet, so every resource can be released directly */
    ai_client_delete(h->ai);
    chat_history_delete(h->history);
    if (h->jobs != NULL) {
        vQueueDelete(h->jobs);
    }
    if (h->lock != NULL) {
        vSemaphoreDelete(h->lock);
    }
    free(h->cfg);
    free(h->prompt);
    free(h->transcript);
    free(h->reply);
    free(h);
    return err;
}

/* Callers' tasks must not touch the worker-owned config copy */
static bool chirps_enabled(void)
{
    app_config_t *cfg = app_config_alloc();
    bool on = false;
    if (cfg != NULL) {
        app_config_get(cfg);
        on = cfg->chirps;
        free(cfg);
    }
    return on;
}

/* A new request may start when idle, or while a reply is only being spoken */
static bool can_start_locked(assistant_handle_t h)
{
    return !h->listening && (h->pending == 0 || h->speaking);
}

esp_err_t assistant_listen_start(assistant_handle_t h)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    xSemaphoreTake(h->lock, portMAX_DELAY);
    if (!can_start_locked(h)) {
        const bool already = h->listening;
        xSemaphoreGive(h->lock);
        return already ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    h->stop_speaking = true;
    h->recorded = false;   /* a new recording replaces a held one */
    esp_err_t err = audio_io_record_start(h->config.audio, chirps_enabled());
    h->listening = err == ESP_OK;
    xSemaphoreGive(h->lock);
    ESP_RETURN_ON_ERROR(err, TAG, "record start");
    emit(h, ASSISTANT_EVT_MODE, ASSISTANT_MODE_LISTENING, NULL);
    return ESP_OK;
}

/* Ends the recording; on success *job holds speech long enough to send */
static esp_err_t stop_capture(assistant_handle_t h, job_t *job)
{
    xSemaphoreTake(h->lock, portMAX_DELAY);
    if (!h->listening) {
        xSemaphoreGive(h->lock);
        return ESP_ERR_INVALID_STATE;
    }
    h->listening = false;
    xSemaphoreGive(h->lock);

    *job = (job_t) { .type = JOB_VOICE, .speak = true };
    esp_err_t err = audio_io_record_stop(h->config.audio, &job->pcm, &job->samples);
    if (err != ESP_OK || job->samples < audio_io_record_rate(h->config.audio) * MIN_SPEECH_MS / 1000) {
        emit(h, ASSISTANT_EVT_MODE, ASSISTANT_MODE_IDLE, NULL);
        return err != ESP_OK ? err : ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static esp_err_t queue_voice(assistant_handle_t h, const job_t *job)
{
    xSemaphoreTake(h->lock, portMAX_DELAY);
    h->pending++;
    xSemaphoreGive(h->lock);
    if (xQueueSend(h->jobs, job, pdMS_TO_TICKS(2000)) != pdTRUE) {
        finish_job(h);
        fail(h, "I'm still busy, please try again.");
        return ESP_ERR_TIMEOUT;
    }
    emit(h, ASSISTANT_EVT_MODE, ASSISTANT_MODE_THINKING, NULL);
    return ESP_OK;
}

esp_err_t assistant_listen_stop(assistant_handle_t h)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    job_t job;
    esp_err_t err = stop_capture(h, &job);
    return err == ESP_OK ? queue_voice(h, &job) : err;
}

esp_err_t assistant_listen_hold(assistant_handle_t h)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    job_t job;
    ESP_RETURN_ON_ERROR(stop_capture(h, &job), TAG, "stop capture");
    xSemaphoreTake(h->lock, portMAX_DELAY);
    h->recorded = true;
    h->recorded_pcm = job.pcm;
    h->recorded_samples = job.samples;
    xSemaphoreGive(h->lock);
    emit(h, ASSISTANT_EVT_MODE, ASSISTANT_MODE_RECORDED, NULL);
    return ESP_OK;
}

esp_err_t assistant_send_recording(assistant_handle_t h)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    xSemaphoreTake(h->lock, portMAX_DELAY);
    if (!h->recorded || h->listening || h->pending > 0) {
        xSemaphoreGive(h->lock);
        return ESP_ERR_INVALID_STATE;
    }
    h->recorded = false;
    const job_t job = {
        .type = JOB_VOICE, .speak = true, .pcm = h->recorded_pcm, .samples = h->recorded_samples,
    };
    xSemaphoreGive(h->lock);
    return queue_voice(h, &job);
}

esp_err_t assistant_listen_toggle(assistant_handle_t h)
{
    return assistant_is_listening(h) ? assistant_listen_stop(h) : assistant_listen_start(h);
}

void assistant_listen_cancel(assistant_handle_t h)
{
    if (h == NULL) {
        return;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    const bool was_listening = h->listening;
    const bool was_recorded = h->recorded;
    h->listening = false;
    h->recorded = false;
    xSemaphoreGive(h->lock);
    if (was_listening) {
        const int16_t *pcm;
        size_t samples;
        audio_io_record_stop(h->config.audio, &pcm, &samples);
    }
    if (was_listening || was_recorded) {
        emit(h, ASSISTANT_EVT_MODE, ASSISTANT_MODE_IDLE, NULL);
    }
}

esp_err_t assistant_ask(assistant_handle_t h, const assistant_question_t *q)
{
    ESP_RETURN_ON_FALSE(h != NULL && q != NULL, ESP_ERR_INVALID_ARG, TAG, "bad args");
    const bool has_text = q->text != NULL && q->text[0] != '\0';
    const bool has_photo = q->jpeg != NULL && q->jpeg_len > 0;
    ESP_RETURN_ON_FALSE(has_text || has_photo, ESP_ERR_INVALID_ARG, TAG, "empty question");

    job_t job = { .type = JOB_TEXT, .speak = q->speak };
    job.text = has_text ? strndup(q->text, TEXT_INPUT_MAX) : strdup("");
    if (has_photo) {
        job.jpeg = heap_caps_malloc(q->jpeg_len, MALLOC_CAP_SPIRAM);
        if (job.jpeg != NULL) {
            memcpy(job.jpeg, q->jpeg, q->jpeg_len);
            job.jpeg_len = q->jpeg_len;
        }
    }
    if (job.text == NULL || (has_photo && job.jpeg == NULL)) {
        free(job.text);
        free(job.jpeg);
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(h->lock, portMAX_DELAY);
    if (!can_start_locked(h)) {
        xSemaphoreGive(h->lock);
        free(job.text);
        free(job.jpeg);
        return ESP_ERR_INVALID_STATE;
    }
    h->stop_speaking = true;
    h->recorded = false;   /* a typed question replaces a held recording */
    audio_io_abort_playback(h->config.audio);
    h->pending++;
    xSemaphoreGive(h->lock);

    if (xQueueSend(h->jobs, &job, pdMS_TO_TICKS(2000)) != pdTRUE) {
        free(job.text);
        free(job.jpeg);
        finish_job(h);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t assistant_send_text(assistant_handle_t h, const char *text)
{
    const assistant_question_t q = { .text = text, .speak = true };
    return assistant_ask(h, &q);
}

void assistant_stop_speaking(assistant_handle_t h)
{
    if (h != NULL && h->speaking) {
        h->stop_speaking = true;
        audio_io_abort_playback(h->config.audio);
    }
}

void assistant_clear_history(assistant_handle_t h)
{
    if (h != NULL) {
        chat_history_clear(h->history);
    }
}

size_t assistant_history_list(assistant_handle_t h, chat_history_entry_t *out, size_t out_max)
{
    return h != NULL ? chat_history_list(h->history, out, out_max) : 0;
}

esp_err_t assistant_history_remove(assistant_handle_t h, uint32_t id)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    return chat_history_remove(h->history, id);
}

bool assistant_is_listening(assistant_handle_t h)
{
    if (h == NULL) {
        return false;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    const bool listening = h->listening;
    xSemaphoreGive(h->lock);
    return listening;
}

bool assistant_has_recording(assistant_handle_t h)
{
    if (h == NULL) {
        return false;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    const bool recorded = h->recorded;
    xSemaphoreGive(h->lock);
    return recorded;
}
