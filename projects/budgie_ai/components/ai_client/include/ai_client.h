// SPDX-License-Identifier: Apache-2.0
//
// Minimal client for OpenAI-compatible HTTP APIs:
//   POST {base}/chat/completions       chat replies
//   POST {base}/audio/transcriptions   speech to text (multipart WAV upload)
//   POST {base}/audio/speech           text to speech (WAV or raw PCM stream)
//
// Works with OpenAI and compatible servers (OpenRouter, Groq, LM Studio,
// Ollama, LocalAI, vLLM...). Both https:// (ESP-IDF certificate bundle) and
// plain http:// base URLs are accepted.
//
// Threading: a handle is single-owner. Calls block until the request finishes
// and must come from one task at a time (the assistant worker task).
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ai_client_t *ai_client_handle_t;

typedef struct {
    size_t response_buf_size;   /* Max JSON response kept in memory (bytes) */
    int timeout_ms;             /* Network timeout for each request */
} ai_client_config_t;

#define AI_CLIENT_DEFAULT_CONFIG() { \
    .response_buf_size = 32 * 1024,  \
    .timeout_ms = 60000,             \
}

typedef enum {
    AI_ROLE_SYSTEM = 0,
    AI_ROLE_USER,
    AI_ROLE_ASSISTANT,
} ai_role_t;

typedef struct {
    ai_role_t role;
    const char *content;
    /* Optional JPEG attached to a user message (sent as an OpenAI "image_url"
     * data URL; needs a vision-capable model). Borrowed for the call. */
    const uint8_t *image_jpeg;
    size_t image_len;
} ai_message_t;

/* Where and as whom to send a request. Strings are borrowed for the call. */
typedef struct {
    const char *base_url;       /* e.g. "https://api.openai.com/v1" (no trailing '/') */
    const char *api_key;        /* sent as "Authorization: Bearer <key>"; may be empty */
    const char *model;
} ai_endpoint_t;

typedef struct {
    ai_endpoint_t endpoint;
    uint16_t max_tokens;
    float temperature;
} ai_chat_params_t;

/**
 * Receives decoded 16-bit mono PCM from ai_client_speech(). @p pcm is valid
 * only during the call. Return ESP_OK to continue, anything else to abort.
 */
typedef esp_err_t (*ai_pcm_sink_t)(const int16_t *pcm, size_t samples,
                                   uint32_t sample_rate, void *user_ctx);

esp_err_t ai_client_create(const ai_client_config_t *config, ai_client_handle_t *ret_handle);
void ai_client_delete(ai_client_handle_t handle);

/** Send @p messages and copy the assistant reply text into @p reply. */
esp_err_t ai_client_chat(ai_client_handle_t handle, const ai_chat_params_t *params,
                         const ai_message_t *messages, size_t message_count,
                         char *reply, size_t reply_size);

/** Upload 16-bit mono PCM as WAV and copy the transcript into @p text. */
esp_err_t ai_client_transcribe(ai_client_handle_t handle, const ai_endpoint_t *endpoint,
                               const char *language, const int16_t *pcm, size_t samples,
                               uint32_t sample_rate, char *text, size_t text_size);

/** Synthesize @p text and stream the audio to @p sink as it arrives. */
esp_err_t ai_client_speech(ai_client_handle_t handle, const ai_endpoint_t *endpoint,
                           const char *voice, const char *text,
                           ai_pcm_sink_t sink, void *user_ctx);

/** List model ids from GET {base}/models into @p out as a JSON array string. */
esp_err_t ai_client_list_models(ai_client_handle_t handle, const ai_endpoint_t *endpoint,
                                char *out, size_t out_size);

/** Human-readable description of the last failure ("" after success). */
const char *ai_client_last_error(ai_client_handle_t handle);

#ifdef __cplusplus
}
#endif
