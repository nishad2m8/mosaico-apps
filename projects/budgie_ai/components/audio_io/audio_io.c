// SPDX-License-Identifier: Apache-2.0
//
// codec_lock serializes every codec open/read/write/close. The capture task
// owns the microphone; playback runs in the caller's task. A playback "session"
// spans the audio_io_play() calls of one clip until audio_io_play_end().

#include "audio_io.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bsp/esp_mosaico.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "audio_io";

#define CAPTURE_TASK_STACK    4096
#define CAPTURE_TASK_PRIO     6
#define CAPTURE_CHUNK_MS      20
#define PLAY_CHUNK_SAMPLES    1024
#define STOP_WAIT_MS          2000
#define BITS_PER_SAMPLE       16
#define CHIRP_RATE            16000
#define CHIRP_TONE1_HZ        1800.0f
#define CHIRP_TONE2_HZ        2600.0f
#define CHIRP_TONE1_MS        60
#define CHIRP_TONE2_MS        80
#define CHIRP_AMPLITUDE       9000.0f
#define CHIRP_FADE_SAMPLES    80
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct audio_io_t {
    audio_io_config_t config;
    esp_codec_dev_handle_t mic;
    esp_codec_dev_handle_t spk;
    SemaphoreHandle_t codec_lock;
    SemaphoreHandle_t record_done;
    TaskHandle_t capture_task;

    int16_t *record_buf;
    size_t record_capacity;          /* samples */
    volatile size_t recorded;        /* samples, written by the capture task */
    volatile bool recording;         /* a recording was started and not yet collected */
    volatile bool stop_request;
    volatile bool exit_request;
    bool chirp_first;

    volatile bool playing;           /* inside a playback session */
    volatile bool abort_play;
    uint32_t spk_rate;               /* 0 when the speaker is closed */
    uint8_t volume;

    int16_t *chirp;
    size_t chirp_samples;

    audio_io_record_full_cb_t full_cb;
    void *full_cb_ctx;
};

/* ---------- Speaker (codec_lock held) ---------- */

static void speaker_close_locked(audio_io_handle_t h)
{
    if (h->spk_rate != 0) {
        esp_codec_dev_close(h->spk);
        h->spk_rate = 0;
    }
}

static esp_err_t speaker_open_locked(audio_io_handle_t h, uint32_t rate)
{
    if (h->spk_rate == rate) {
        return ESP_OK;
    }
    speaker_close_locked(h);
    esp_codec_dev_sample_info_t fmt = {
        .sample_rate = rate,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel = 1,
        .channel_mask = 0,
    };
    ESP_RETURN_ON_FALSE(esp_codec_dev_open(h->spk, &fmt) == ESP_CODEC_DEV_OK, ESP_FAIL,
                        TAG, "speaker open at %u Hz failed", (unsigned)rate);
    esp_codec_dev_set_out_vol(h->spk, h->volume);
    h->spk_rate = rate;
    return ESP_OK;
}

/* ---------- Capture task ---------- */

static void capture_one(audio_io_handle_t h)
{
    xSemaphoreTake(h->codec_lock, portMAX_DELAY);
    speaker_close_locked(h);   /* shared I2S: never both directions at once */

    esp_codec_dev_sample_info_t fmt = {
        .sample_rate = h->config.record_rate,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel = 1,
        .channel_mask = 0,
    };
    if (esp_codec_dev_open(h->mic, &fmt) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "microphone open failed");
        xSemaphoreGive(h->codec_lock);
        return;
    }
    esp_codec_dev_set_in_gain(h->mic, h->config.mic_gain_db);

    const size_t chunk = h->config.record_rate * CAPTURE_CHUNK_MS / 1000;
    while (!h->stop_request && h->recorded + chunk <= h->record_capacity) {
        int16_t *dst = h->record_buf + h->recorded;
        if (esp_codec_dev_read(h->mic, dst, (int)(chunk * sizeof(int16_t))) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "microphone read failed");
            break;
        }
        h->recorded += chunk;
    }
    esp_codec_dev_close(h->mic);
    xSemaphoreGive(h->codec_lock);
}

static void capture_task(void *arg)
{
    audio_io_handle_t h = arg;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (h->exit_request) {
            break;
        }
        if (h->chirp_first) {
            audio_io_chirp(h);
        }
        capture_one(h);
        const bool hit_limit = !h->stop_request;
        xSemaphoreGive(h->record_done);
        if (hit_limit && h->full_cb != NULL) {
            h->full_cb(h->full_cb_ctx);
        }
    }
    h->capture_task = NULL;
    vTaskDelete(NULL);
}

/* ---------- Chirp ---------- */

static void make_tone(int16_t *out, size_t n, float hz)
{
    for (size_t i = 0; i < n; i++) {
        float env = 1.0f;
        if (i < CHIRP_FADE_SAMPLES) {
            env = (float)i / CHIRP_FADE_SAMPLES;
        } else if (n - i < CHIRP_FADE_SAMPLES) {
            env = (float)(n - i) / CHIRP_FADE_SAMPLES;
        }
        out[i] = (int16_t)(CHIRP_AMPLITUDE * env * sinf(2.0f * (float)M_PI * hz * (float)i / CHIRP_RATE));
    }
}

static esp_err_t build_chirp(audio_io_handle_t h)
{
    const size_t n1 = CHIRP_RATE * CHIRP_TONE1_MS / 1000;
    const size_t n2 = CHIRP_RATE * CHIRP_TONE2_MS / 1000;
    h->chirp = heap_caps_malloc((n1 + n2) * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(h->chirp != NULL, ESP_ERR_NO_MEM, TAG, "no memory for chirp");
    make_tone(h->chirp, n1, CHIRP_TONE1_HZ);
    make_tone(h->chirp + n1, n2, CHIRP_TONE2_HZ);
    h->chirp_samples = n1 + n2;
    return ESP_OK;
}

/* ---------- Public API ---------- */

esp_err_t audio_io_create(const audio_io_config_t *config, audio_io_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config != NULL && ret_handle != NULL && config->record_rate > 0 &&
                        config->max_record_ms > 0, ESP_ERR_INVALID_ARG, TAG, "bad args");

    audio_io_handle_t h = calloc(1, sizeof(*h));
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    h->config = *config;
    h->volume = config->volume > 100 ? 100 : config->volume;

    esp_err_t err = ESP_ERR_NO_MEM;
    h->codec_lock = xSemaphoreCreateMutex();
    h->record_done = xSemaphoreCreateBinary();
    h->record_capacity = (size_t)config->record_rate * config->max_record_ms / 1000;
    h->record_buf = heap_caps_malloc(h->record_capacity * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (h->codec_lock == NULL || h->record_done == NULL || h->record_buf == NULL) {
        goto fail;
    }
    if ((err = build_chirp(h)) != ESP_OK) {
        goto fail;
    }

    h->spk = bsp_audio_codec_speaker_init();
    h->mic = bsp_audio_codec_microphone_init();
    if (h->spk == NULL || h->mic == NULL) {
        ESP_LOGE(TAG, "codec init failed");
        err = ESP_FAIL;
        goto fail;
    }

    if (xTaskCreate(capture_task, "audio_capture", CAPTURE_TASK_STACK, h, CAPTURE_TASK_PRIO,
                    &h->capture_task) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    *ret_handle = h;
    return ESP_OK;

fail:
    audio_io_delete(h);
    return err;
}

void audio_io_delete(audio_io_handle_t h)
{
    if (h == NULL) {
        return;
    }
    if (h->capture_task != NULL) {
        h->stop_request = true;
        h->exit_request = true;
        xTaskNotifyGive(h->capture_task);
        for (int i = 0; i < 50 && h->capture_task != NULL; i++) {
            vTaskDelay(pdMS_TO_TICKS(CAPTURE_CHUNK_MS));
        }
    }
    if (h->codec_lock != NULL) {
        xSemaphoreTake(h->codec_lock, portMAX_DELAY);
        speaker_close_locked(h);
        xSemaphoreGive(h->codec_lock);
        vSemaphoreDelete(h->codec_lock);
    }
    if (h->record_done != NULL) {
        vSemaphoreDelete(h->record_done);
    }
    /* Codec handles are BSP singletons and stay alive */
    free(h->record_buf);
    free(h->chirp);
    free(h);
}

void audio_io_register_cb(audio_io_handle_t h, audio_io_record_full_cb_t cb, void *user_ctx)
{
    if (h != NULL) {
        h->full_cb = cb;
        h->full_cb_ctx = user_ctx;
    }
}

esp_err_t audio_io_record_start(audio_io_handle_t h, bool chirp_first)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    ESP_RETURN_ON_FALSE(!h->recording, ESP_ERR_INVALID_STATE, TAG, "already recording");
    if (h->playing) {
        h->abort_play = true;
    }
    xSemaphoreTake(h->record_done, 0);   /* drop a stale completion */
    h->recorded = 0;
    h->stop_request = false;
    h->chirp_first = chirp_first;
    h->recording = true;
    xTaskNotifyGive(h->capture_task);
    return ESP_OK;
}

esp_err_t audio_io_record_stop(audio_io_handle_t h, const int16_t **pcm, size_t *samples)
{
    ESP_RETURN_ON_FALSE(h != NULL && pcm != NULL && samples != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "bad args");
    *pcm = h->record_buf;
    *samples = 0;
    ESP_RETURN_ON_FALSE(h->recording, ESP_ERR_INVALID_STATE, TAG, "not recording");
    h->stop_request = true;
    const bool done = xSemaphoreTake(h->record_done, pdMS_TO_TICKS(STOP_WAIT_MS)) == pdTRUE;
    h->recording = false;
    ESP_RETURN_ON_FALSE(done, ESP_ERR_TIMEOUT, TAG, "capture did not stop");
    *samples = h->recorded;
    return ESP_OK;
}

bool audio_io_is_recording(audio_io_handle_t h)
{
    return h != NULL && h->recording;
}

uint32_t audio_io_record_rate(audio_io_handle_t h)
{
    return h != NULL ? h->config.record_rate : 0;
}

esp_err_t audio_io_play(audio_io_handle_t h, const int16_t *pcm, size_t samples, uint32_t sample_rate)
{
    ESP_RETURN_ON_FALSE(h != NULL && pcm != NULL && sample_rate > 0, ESP_ERR_INVALID_ARG,
                        TAG, "bad args");
    h->playing = true;
    if (h->abort_play) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(h->codec_lock, portMAX_DELAY);
    esp_err_t err = speaker_open_locked(h, sample_rate);
    for (size_t off = 0; err == ESP_OK && off < samples; off += PLAY_CHUNK_SAMPLES) {
        if (h->abort_play) {
            err = ESP_ERR_INVALID_STATE;
            break;
        }
        const size_t n = (samples - off) < PLAY_CHUNK_SAMPLES ? (samples - off) : PLAY_CHUNK_SAMPLES;
        if (esp_codec_dev_write(h->spk, (void *)(pcm + off), (int)(n * sizeof(int16_t))) !=
                ESP_CODEC_DEV_OK) {
            err = ESP_FAIL;
        }
    }
    xSemaphoreGive(h->codec_lock);
    return err;
}

void audio_io_play_end(audio_io_handle_t h)
{
    if (h == NULL) {
        return;
    }
    xSemaphoreTake(h->codec_lock, portMAX_DELAY);
    speaker_close_locked(h);
    h->playing = false;
    h->abort_play = false;
    xSemaphoreGive(h->codec_lock);
}

void audio_io_abort_playback(audio_io_handle_t h)
{
    if (h != NULL && h->playing) {
        h->abort_play = true;
    }
}

esp_err_t audio_io_chirp(audio_io_handle_t h)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    /* Independent of the playback session flags, so it never clears an abort
     * aimed at a reply that is being interrupted */
    xSemaphoreTake(h->codec_lock, portMAX_DELAY);
    esp_err_t err = speaker_open_locked(h, CHIRP_RATE);
    if (err == ESP_OK &&
            esp_codec_dev_write(h->spk, h->chirp, (int)(h->chirp_samples * sizeof(int16_t))) !=
            ESP_CODEC_DEV_OK) {
        err = ESP_FAIL;
    }
    speaker_close_locked(h);
    xSemaphoreGive(h->codec_lock);
    return err;
}

esp_err_t audio_io_set_volume(audio_io_handle_t h, uint8_t volume)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    xSemaphoreTake(h->codec_lock, portMAX_DELAY);
    h->volume = volume > 100 ? 100 : volume;
    if (h->spk_rate != 0) {
        esp_codec_dev_set_out_vol(h->spk, h->volume);
    }
    xSemaphoreGive(h->codec_lock);
    return ESP_OK;
}
