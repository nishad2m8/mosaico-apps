// SPDX-License-Identifier: Apache-2.0
//
// Persistent application settings (Wi-Fi, AI service, device preferences).
//
// app_config is a process-wide singleton: app_config_init() loads the values
// from the default "nvs" partition once, after nvs_flash_init(). Every other
// call is thread-safe; readers get a private copy, writers replace the stored
// copy and persist it. app_config_t is ~2.6 KB, so never place it on a task
// stack: use app_config_alloc() or a static/instance-owned buffer.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_CONFIG_SSID_LEN      33   /* 32 chars + NUL (802.11 limit) */
#define APP_CONFIG_PASS_LEN      65   /* 64 chars + NUL (WPA2 limit) */
#define APP_CONFIG_URL_LEN       192
#define APP_CONFIG_KEY_LEN       224
#define APP_CONFIG_MODEL_LEN     64
#define APP_CONFIG_VOICE_LEN     32
#define APP_CONFIG_PROMPT_LEN    1024
#define APP_CONFIG_TZ_LEN        48   /* POSIX TZ string, e.g. "CET-1CEST,M3.5.0,M10.5.0/3" */
#define APP_CONFIG_TITLE_LEN     17   /* status bar title: 16 chars + NUL, fits between clock and battery */
#define APP_CONFIG_SUBTITLE_LEN  41   /* home line under the status bar: 40 chars + NUL */

#define APP_CONFIG_TEMPERATURE_MAX_X10  20
#define APP_CONFIG_MAX_TOKENS_MIN       16
#define APP_CONFIG_MAX_TOKENS_MAX       4096
#define APP_CONFIG_HISTORY_TURNS_MAX    16

/* Values of app_config_t.screen_timeout, matching the Settings screen choice */
typedef enum {
    APP_SCREEN_TIMEOUT_30S = 0,
    APP_SCREEN_TIMEOUT_1MIN,
    APP_SCREEN_TIMEOUT_5MIN,
    APP_SCREEN_TIMEOUT_NEVER,
} app_screen_timeout_t;

/* Values of app_config_t.clock_format, matching the Settings screen choice */
typedef enum {
    APP_CLOCK_FORMAT_12H = 0,
    APP_CLOCK_FORMAT_24H,
} app_clock_format_t;

typedef struct {
    /* Wi-Fi station credentials; empty SSID means "not provisioned" */
    char wifi_ssid[APP_CONFIG_SSID_LEN];
    char wifi_pass[APP_CONFIG_PASS_LEN];

    /* OpenAI-compatible service. base_url ends before the endpoint, e.g.
     * "https://api.openai.com/v1"; endpoints are appended as "/chat/completions". */
    char api_base_url[APP_CONFIG_URL_LEN];
    char api_key[APP_CONFIG_KEY_LEN];
    char chat_model[APP_CONFIG_MODEL_LEN];
    char vision_model[APP_CONFIG_MODEL_LEN];   /* photo questions; empty = chat_model */
    char system_prompt[APP_CONFIG_PROMPT_LEN];
    uint16_t max_tokens;
    uint8_t temperature_x10;      /* 0..20 -> 0.0..2.0 */
    uint8_t history_turns;        /* user+assistant pairs kept as context */

    /* Speech services, optionally on a different OpenAI-compatible host.
     * Empty speech_base_url / speech_api_key reuse the chat values. */
    char speech_base_url[APP_CONFIG_URL_LEN];
    char speech_api_key[APP_CONFIG_KEY_LEN];
    char stt_model[APP_CONFIG_MODEL_LEN];
    char stt_language[8];         /* ISO-639-1 hint, empty = auto */
    char tts_model[APP_CONFIG_MODEL_LEN];
    char tts_voice[APP_CONFIG_VOICE_LEN];
    bool tts_enabled;

    /* Device preferences (mirrored by the on-device Settings screen) */
    uint8_t volume;               /* 0..100 % */
    uint8_t brightness;           /* 10..100 % */
    uint8_t screen_timeout;       /* app_screen_timeout_t */
    uint8_t clock_format;         /* app_clock_format_t */
    char timezone[APP_CONFIG_TZ_LEN];
    char title[APP_CONFIG_TITLE_LEN];         /* status bar title */
    char subtitle[APP_CONFIG_SUBTITLE_LEN];   /* home line under the status bar; empty hides it */
    bool chirps;
    bool kid_safe;
} app_config_t;

/** Load settings from NVS (defaults for missing keys). Call once at boot. */
esp_err_t app_config_init(void);

/** Allocate a zeroed app_config_t (PSRAM preferred). Free with free(). */
app_config_t *app_config_alloc(void);

/** Copy the current settings into @p out. */
void app_config_get(app_config_t *out);

/** Validate, store and persist @p cfg, then notify the change callback. */
esp_err_t app_config_set(const app_config_t *cfg);

/** Restore defaults for everything (Wi-Fi credentials included) and persist. */
esp_err_t app_config_reset(void);

/** True when an API key and base URL are configured. */
bool app_config_ai_ready(void);

/**
 * Change notification, called from the task that invoked app_config_set()
 * after the new values are stored. Keep it short; do not call app_config_set()
 * from it. Only one callback is supported; pass NULL to unregister.
 */
typedef void (*app_config_change_cb_t)(void *user_ctx);
void app_config_register_cb(app_config_change_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif
