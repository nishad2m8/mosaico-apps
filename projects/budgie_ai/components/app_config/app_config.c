// SPDX-License-Identifier: Apache-2.0
//
// Settings are stored one NVS key per field (namespace "budgie"), described by
// the field tables below, so adding a field never invalidates stored values.

#include "app_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

static const char *TAG = "app_config";

#define APP_CONFIG_NAMESPACE "budgie"

#define DEFAULT_BASE_URL      "https://api.openai.com/v1"
#define DEFAULT_CHAT_MODEL    "gpt-4o-mini"
#define DEFAULT_STT_MODEL     "whisper-1"
#define DEFAULT_TTS_MODEL     "gpt-4o-mini-tts"
#define DEFAULT_TTS_VOICE     "alloy"
#define DEFAULT_TIMEZONE      "UTC0"
#define DEFAULT_TITLE         "Budgie"
#define DEFAULT_SUBTITLE      "Personal AI Assistant"
#define DEFAULT_SYSTEM_PROMPT \
    "You are Budgie, a cheerful little parrot assistant living on a small round " \
    "screen. Answer in plain text without markdown, in at most three short sentences."

typedef struct {
    const char *key;      /* NVS key, max 15 chars */
    size_t offset;
    size_t size;
} str_field_t;

typedef struct {
    const char *key;
    size_t offset;
    size_t size;          /* 1 (uint8_t/bool) or 2 (uint16_t) */
} num_field_t;

#define STR_FIELD(k, m) { k, offsetof(app_config_t, m), sizeof(((app_config_t *)0)->m) }
#define NUM_FIELD(k, m) { k, offsetof(app_config_t, m), sizeof(((app_config_t *)0)->m) }

static const str_field_t s_str_fields[] = {
    STR_FIELD("wifi_ssid", wifi_ssid),
    STR_FIELD("wifi_pass", wifi_pass),
    STR_FIELD("api_url", api_base_url),
    STR_FIELD("api_key", api_key),
    STR_FIELD("chat_model", chat_model),
    STR_FIELD("vision_model", vision_model),
    STR_FIELD("sys_prompt", system_prompt),
    STR_FIELD("sp_url", speech_base_url),
    STR_FIELD("sp_key", speech_api_key),
    STR_FIELD("stt_model", stt_model),
    STR_FIELD("stt_lang", stt_language),
    STR_FIELD("tts_model", tts_model),
    STR_FIELD("tts_voice", tts_voice),
    STR_FIELD("timezone", timezone),
    STR_FIELD("title", title),
    STR_FIELD("subtitle", subtitle),
};

static const num_field_t s_num_fields[] = {
    NUM_FIELD("max_tokens", max_tokens),
    NUM_FIELD("temp_x10", temperature_x10),
    NUM_FIELD("history", history_turns),
    NUM_FIELD("tts_on", tts_enabled),
    NUM_FIELD("volume", volume),
    NUM_FIELD("brightness", brightness),
    NUM_FIELD("scr_timeout", screen_timeout),
    NUM_FIELD("clock_fmt", clock_format),
    NUM_FIELD("chirps", chirps),
    NUM_FIELD("kid_safe", kid_safe),
};

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static struct {
    SemaphoreHandle_t lock;
    app_config_t *cfg;
    app_config_change_cb_t cb;
    void *cb_ctx;
} s_cfg;

static void set_defaults(app_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    strlcpy(cfg->api_base_url, DEFAULT_BASE_URL, sizeof(cfg->api_base_url));
    strlcpy(cfg->chat_model, DEFAULT_CHAT_MODEL, sizeof(cfg->chat_model));
    strlcpy(cfg->system_prompt, DEFAULT_SYSTEM_PROMPT, sizeof(cfg->system_prompt));
    strlcpy(cfg->stt_model, DEFAULT_STT_MODEL, sizeof(cfg->stt_model));
    strlcpy(cfg->tts_model, DEFAULT_TTS_MODEL, sizeof(cfg->tts_model));
    strlcpy(cfg->tts_voice, DEFAULT_TTS_VOICE, sizeof(cfg->tts_voice));
    strlcpy(cfg->timezone, DEFAULT_TIMEZONE, sizeof(cfg->timezone));
    strlcpy(cfg->title, DEFAULT_TITLE, sizeof(cfg->title));
    strlcpy(cfg->subtitle, DEFAULT_SUBTITLE, sizeof(cfg->subtitle));
    cfg->max_tokens = 300;
    cfg->temperature_x10 = 7;
    cfg->history_turns = 6;
    cfg->tts_enabled = true;
    cfg->volume = 70;
    cfg->brightness = 80;
    cfg->screen_timeout = APP_SCREEN_TIMEOUT_1MIN;
    cfg->clock_format = APP_CLOCK_FORMAT_24H;
    cfg->chirps = true;
    cfg->kid_safe = false;
}

static void clamp(app_config_t *cfg)
{
    if (cfg->temperature_x10 > APP_CONFIG_TEMPERATURE_MAX_X10) {
        cfg->temperature_x10 = APP_CONFIG_TEMPERATURE_MAX_X10;
    }
    if (cfg->max_tokens < APP_CONFIG_MAX_TOKENS_MIN) {
        cfg->max_tokens = APP_CONFIG_MAX_TOKENS_MIN;
    } else if (cfg->max_tokens > APP_CONFIG_MAX_TOKENS_MAX) {
        cfg->max_tokens = APP_CONFIG_MAX_TOKENS_MAX;
    }
    if (cfg->history_turns > APP_CONFIG_HISTORY_TURNS_MAX) {
        cfg->history_turns = APP_CONFIG_HISTORY_TURNS_MAX;
    }
    if (cfg->volume > 100) {
        cfg->volume = 100;
    }
    if (cfg->brightness < 10) {
        cfg->brightness = 10;
    } else if (cfg->brightness > 100) {
        cfg->brightness = 100;
    }
    if (cfg->screen_timeout > APP_SCREEN_TIMEOUT_NEVER) {
        cfg->screen_timeout = APP_SCREEN_TIMEOUT_1MIN;
    }
    if (cfg->clock_format > APP_CLOCK_FORMAT_24H) {
        cfg->clock_format = APP_CLOCK_FORMAT_24H;
    }
    /* Strip a trailing '/' so endpoints can be appended uniformly */
    size_t n = strlen(cfg->api_base_url);
    while (n > 0 && cfg->api_base_url[n - 1] == '/') {
        cfg->api_base_url[--n] = '\0';
    }
    n = strlen(cfg->speech_base_url);
    while (n > 0 && cfg->speech_base_url[n - 1] == '/') {
        cfg->speech_base_url[--n] = '\0';
    }
}

static void load(nvs_handle_t nvs, app_config_t *cfg)
{
    for (size_t i = 0; i < ARRAY_LEN(s_str_fields); i++) {
        const str_field_t *f = &s_str_fields[i];
        char *dst = (char *)cfg + f->offset;
        size_t len = f->size;
        if (nvs_get_str(nvs, f->key, dst, &len) != ESP_OK) {
            continue;   /* keep the default; a too-long value is also ignored */
        }
    }
    for (size_t i = 0; i < ARRAY_LEN(s_num_fields); i++) {
        const num_field_t *f = &s_num_fields[i];
        uint8_t *dst = (uint8_t *)cfg + f->offset;
        if (f->size == sizeof(uint16_t)) {
            uint16_t v;
            if (nvs_get_u16(nvs, f->key, &v) == ESP_OK) {
                memcpy(dst, &v, sizeof(v));
            }
        } else {
            uint8_t v;
            if (nvs_get_u8(nvs, f->key, &v) == ESP_OK) {
                *dst = v;
            }
        }
    }
}

static esp_err_t save(const app_config_t *cfg)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(APP_CONFIG_NAMESPACE, NVS_READWRITE, &nvs), TAG, "nvs_open");

    esp_err_t err = ESP_OK;
    for (size_t i = 0; i < ARRAY_LEN(s_str_fields) && err == ESP_OK; i++) {
        const str_field_t *f = &s_str_fields[i];
        err = nvs_set_str(nvs, f->key, (const char *)cfg + f->offset);
    }
    for (size_t i = 0; i < ARRAY_LEN(s_num_fields) && err == ESP_OK; i++) {
        const num_field_t *f = &s_num_fields[i];
        const uint8_t *src = (const uint8_t *)cfg + f->offset;
        if (f->size == sizeof(uint16_t)) {
            uint16_t v;
            memcpy(&v, src, sizeof(v));
            err = nvs_set_u16(nvs, f->key, v);
        } else {
            err = nvs_set_u8(nvs, f->key, *src);
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    }
    return err;
}

app_config_t *app_config_alloc(void)
{
    app_config_t *cfg = heap_caps_calloc(1, sizeof(*cfg), MALLOC_CAP_SPIRAM);
    if (cfg == NULL) {
        cfg = calloc(1, sizeof(*cfg));
    }
    return cfg;
}

esp_err_t app_config_init(void)
{
    ESP_RETURN_ON_FALSE(s_cfg.lock == NULL, ESP_ERR_INVALID_STATE, TAG, "already initialized");

    app_config_t *cfg = app_config_alloc();
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_NO_MEM, TAG, "no memory for config");
    SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    if (lock == NULL) {
        free(cfg);
        return ESP_ERR_NO_MEM;
    }

    set_defaults(cfg);
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(APP_CONFIG_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        load(nvs, cfg);
        nvs_close(nvs);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "nvs_open: %s; using defaults", esp_err_to_name(err));
    }
    clamp(cfg);

    s_cfg.cfg = cfg;
    s_cfg.lock = lock;
    ESP_LOGI(TAG, "loaded (wifi %s, ai %s)", cfg->wifi_ssid[0] ? "set" : "unset",
             cfg->api_key[0] ? "set" : "unset");
    return ESP_OK;
}

void app_config_get(app_config_t *out)
{
    if (out == NULL || s_cfg.lock == NULL) {
        return;
    }
    xSemaphoreTake(s_cfg.lock, portMAX_DELAY);
    memcpy(out, s_cfg.cfg, sizeof(*out));
    xSemaphoreGive(s_cfg.lock);
}

esp_err_t app_config_set(const app_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(s_cfg.lock != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialized");

    xSemaphoreTake(s_cfg.lock, portMAX_DELAY);
    memcpy(s_cfg.cfg, cfg, sizeof(*cfg));
    s_cfg.cfg->wifi_ssid[APP_CONFIG_SSID_LEN - 1] = '\0';
    clamp(s_cfg.cfg);
    esp_err_t err = save(s_cfg.cfg);
    app_config_change_cb_t cb = s_cfg.cb;
    void *cb_ctx = s_cfg.cb_ctx;
    xSemaphoreGive(s_cfg.lock);

    if (cb != NULL) {
        cb(cb_ctx);
    }
    return err;
}

esp_err_t app_config_reset(void)
{
    app_config_t *cfg = app_config_alloc();
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    set_defaults(cfg);
    esp_err_t err = app_config_set(cfg);
    free(cfg);
    return err;
}

bool app_config_ai_ready(void)
{
    if (s_cfg.lock == NULL) {
        return false;
    }
    xSemaphoreTake(s_cfg.lock, portMAX_DELAY);
    bool ready = s_cfg.cfg->api_key[0] != '\0' && s_cfg.cfg->api_base_url[0] != '\0';
    xSemaphoreGive(s_cfg.lock);
    return ready;
}

void app_config_register_cb(app_config_change_cb_t cb, void *user_ctx)
{
    if (s_cfg.lock == NULL) {
        s_cfg.cb = cb;
        s_cfg.cb_ctx = user_ctx;
        return;
    }
    xSemaphoreTake(s_cfg.lock, portMAX_DELAY);
    s_cfg.cb = cb;
    s_cfg.cb_ctx = user_ctx;
    xSemaphoreGive(s_cfg.lock);
}
