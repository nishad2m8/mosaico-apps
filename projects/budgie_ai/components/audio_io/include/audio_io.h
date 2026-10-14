// SPDX-License-Identifier: Apache-2.0
//
// Half-duplex audio on the ESP-Mosaico ES8311 codec: microphone capture into
// a PSRAM buffer and blocking speaker playback at any sample rate.
//
// The microphone and speaker share one I2S bus, so only one direction is open
// at a time. Starting a recording aborts any playback in progress.
//
// Threading: record_start/stop, abort and set_volume may be called from any
// task. play/play_end/chirp block and are meant for one playback task at a time.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_io_t *audio_io_handle_t;

typedef struct {
    uint32_t record_rate;        /* microphone sample rate (Hz) */
    uint32_t max_record_ms;      /* capture stops automatically at this length */
    float mic_gain_db;
    uint8_t volume;              /* initial speaker volume 0..100 */
} audio_io_config_t;

#define AUDIO_IO_DEFAULT_CONFIG() { \
    .record_rate = 16000,           \
    .max_record_ms = 20000,         \
    .mic_gain_db = 30.0f,           \
    .volume = 70,                   \
}

/** Called from the capture task when a recording ends by itself (length limit). */
typedef void (*audio_io_record_full_cb_t)(void *user_ctx);

esp_err_t audio_io_create(const audio_io_config_t *config, audio_io_handle_t *ret_handle);
void audio_io_delete(audio_io_handle_t handle);

void audio_io_register_cb(audio_io_handle_t handle, audio_io_record_full_cb_t cb, void *user_ctx);

/** Begin capturing (optionally after a short chirp). Aborts playback first. */
esp_err_t audio_io_record_start(audio_io_handle_t handle, bool chirp_first);

/**
 * Stop capturing and get the recording. @p pcm stays valid until the next
 * audio_io_record_start(). @p samples is 0 if nothing was captured.
 */
esp_err_t audio_io_record_stop(audio_io_handle_t handle, const int16_t **pcm, size_t *samples);

bool audio_io_is_recording(audio_io_handle_t handle);

/** Sample rate of recordings returned by audio_io_record_stop(). */
uint32_t audio_io_record_rate(audio_io_handle_t handle);

/** Play mono 16-bit PCM; returns ESP_ERR_INVALID_STATE if aborted. */
esp_err_t audio_io_play(audio_io_handle_t handle, const int16_t *pcm, size_t samples,
                        uint32_t sample_rate);

/** Close the speaker after the last audio_io_play() of a clip (amplifier off). */
void audio_io_play_end(audio_io_handle_t handle);

/** Make the current/next audio_io_play() return early. Cleared by the next record_start or play_end. */
void audio_io_abort_playback(audio_io_handle_t handle);

/** Short rising two-tone "chirp" (blocking, ~150 ms). */
esp_err_t audio_io_chirp(audio_io_handle_t handle);

esp_err_t audio_io_set_volume(audio_io_handle_t handle, uint8_t volume);

#ifdef __cplusplus
}
#endif
