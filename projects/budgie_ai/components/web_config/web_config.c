// SPDX-License-Identifier: Apache-2.0

#include "web_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ai_client.h"
#include "app_config.h"
#include "cJSON.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "wifi_manager.h"

static const char *TAG = "web_config";

#define HTTPD_STACK          (10 * 1024)   /* /api/test performs a TLS request */
#define HTTPD_MAX_SOCKETS    7
#define HTTPD_MAX_HANDLERS   12
#define BODY_MAX             (8 * 1024)
#define SCAN_MAX             20
#define MODELS_JSON_MAX      (8 * 1024)

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

/* ---------- Settings schema exposed to the page ---------- */

typedef enum {
    FIELD_STR,
    FIELD_SECRET,      /* write-only string; GET reports "<name>_set" */
    FIELD_U8,
    FIELD_U16,
    FIELD_BOOL,
} field_type_t;

typedef struct {
    const char *name;
    field_type_t type;
    size_t offset;
    size_t size;
} field_t;

#define F(n, t, m) { n, t, offsetof(app_config_t, m), sizeof(((app_config_t *)0)->m) }

static const field_t s_fields[] = {
    F("api_base_url", FIELD_STR, api_base_url),
    F("api_key", FIELD_SECRET, api_key),
    F("chat_model", FIELD_STR, chat_model),
    F("vision_model", FIELD_STR, vision_model),
    F("system_prompt", FIELD_STR, system_prompt),
    F("max_tokens", FIELD_U16, max_tokens),
    F("temperature_x10", FIELD_U8, temperature_x10),
    F("history_turns", FIELD_U8, history_turns),
    F("speech_base_url", FIELD_STR, speech_base_url),
    F("speech_api_key", FIELD_SECRET, speech_api_key),
    F("stt_model", FIELD_STR, stt_model),
    F("stt_language", FIELD_STR, stt_language),
    F("tts_enabled", FIELD_BOOL, tts_enabled),
    F("tts_model", FIELD_STR, tts_model),
    F("tts_voice", FIELD_STR, tts_voice),
    F("volume", FIELD_U8, volume),
    F("brightness", FIELD_U8, brightness),
    F("screen_timeout", FIELD_U8, screen_timeout),
    F("clock_format", FIELD_U8, clock_format),
    F("timezone", FIELD_STR, timezone),
    F("title", FIELD_STR, title),
    F("subtitle", FIELD_STR, subtitle),
    F("chirps", FIELD_BOOL, chirps),
    F("kid_safe", FIELD_BOOL, kid_safe),
};

#define FIELD_COUNT (sizeof(s_fields) / sizeof(s_fields[0]))

static struct {
    httpd_handle_t server;
    web_config_config_t config;
} s_web;

/* ---------- Helpers ---------- */

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (text == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, text);
    cJSON_free(text);
    return err;
}

static esp_err_t send_result(httpd_req_t *req, bool ok, const char *error)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    cJSON_AddBoolToObject(root, "ok", ok);
    if (error != NULL) {
        cJSON_AddStringToObject(root, "error", error);
    }
    return send_json(req, root);
}

/* Read and parse a JSON request body. NULL (with a response sent) on failure. */
static cJSON *read_json(httpd_req_t *req)
{
    if (req->content_len == 0 || req->content_len > BODY_MAX) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body missing or too large");
        return NULL;
    }
    char *buf = heap_caps_malloc(req->content_len + 1, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
        return NULL;
    }
    size_t got = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, buf + got, req->content_len - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (r <= 0) {
            free(buf);
            return NULL;
        }
        got += (size_t)r;
    }
    buf[got] = '\0';
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
    }
    return root;
}

static const char *json_str(const cJSON *root, const char *name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static const char *wifi_state_name(wifi_mgr_state_t state)
{
    switch (state) {
    case WIFI_MGR_STATE_CONNECTING:
        return "connecting";
    case WIFI_MGR_STATE_CONNECTED:
        return "connected";
    case WIFI_MGR_STATE_FAILED:
        return "failed";
    case WIFI_MGR_STATE_IDLE:
    default:
        return "not_configured";
    }
}

/* ---------- Handlers ---------- */

static esp_err_t handle_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start);
}

static esp_err_t handle_status(httpd_req_t *req)
{
    wifi_mgr_status_t wifi;
    wifi_mgr_get_status(&wifi);
    web_config_device_info_t info = { .battery_percent = -1, .app_version = "" };
    if (s_web.config.get_device_info != NULL) {
        s_web.config.get_device_info(&info, s_web.config.user_ctx);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *w = cJSON_AddObjectToObject(root, "wifi");
    if (root == NULL || w == NULL) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    cJSON_AddStringToObject(w, "state", wifi_state_name(wifi.state));
    cJSON_AddStringToObject(w, "ssid", wifi.ssid);
    cJSON_AddStringToObject(w, "ip", wifi.ip);
    cJSON_AddNumberToObject(w, "rssi", wifi.rssi);
    cJSON_AddBoolToObject(w, "ap_active", wifi.ap_active);
    cJSON_AddStringToObject(w, "ap_ssid", wifi.ap_ssid);
    cJSON_AddStringToObject(w, "ap_ip", wifi.ap_ip);
    cJSON_AddNumberToObject(root, "battery", info.battery_percent);
    cJSON_AddBoolToObject(root, "charging", info.charging);
    cJSON_AddStringToObject(root, "version", info.app_version != NULL ? info.app_version : "");
    cJSON_AddBoolToObject(root, "ai_ready", app_config_ai_ready());
    cJSON_AddNumberToObject(root, "free_heap", (double)esp_get_free_heap_size());
    return send_json(req, root);
}

static esp_err_t handle_get_config(httpd_req_t *req)
{
    app_config_t *cfg = app_config_alloc();
    cJSON *root = cJSON_CreateObject();
    if (cfg == NULL || root == NULL) {
        free(cfg);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    app_config_get(cfg);

    char set_name[40];
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        const field_t *f = &s_fields[i];
        const uint8_t *p = (const uint8_t *)cfg + f->offset;
        switch (f->type) {
        case FIELD_STR:
            cJSON_AddStringToObject(root, f->name, (const char *)p);
            break;
        case FIELD_SECRET:
            snprintf(set_name, sizeof(set_name), "%s_set", f->name);
            cJSON_AddBoolToObject(root, set_name, ((const char *)p)[0] != '\0');
            break;
        case FIELD_U8:
            cJSON_AddNumberToObject(root, f->name, *p);
            break;
        case FIELD_U16: {
            uint16_t v;
            memcpy(&v, p, sizeof(v));
            cJSON_AddNumberToObject(root, f->name, v);
            break;
        }
        case FIELD_BOOL:
            cJSON_AddBoolToObject(root, f->name, *(const bool *)p);
            break;
        }
    }
    cJSON_AddStringToObject(root, "wifi_ssid", cfg->wifi_ssid);
    free(cfg);
    return send_json(req, root);
}

/* Apply the JSON members that are present. Empty secrets are left unchanged;
 * "<name>_clear": true erases a stored secret. */
static void apply_fields(const cJSON *root, app_config_t *cfg)
{
    char clear_name[40];
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        const field_t *f = &s_fields[i];
        uint8_t *p = (uint8_t *)cfg + f->offset;
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, f->name);
        switch (f->type) {
        case FIELD_SECRET:
            snprintf(clear_name, sizeof(clear_name), "%s_clear", f->name);
            if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, clear_name))) {
                p[0] = '\0';
            }
            if (cJSON_IsString(item) && item->valuestring[0] != '\0') {
                strlcpy((char *)p, item->valuestring, f->size);
            }
            break;
        case FIELD_STR:
            if (cJSON_IsString(item)) {
                strlcpy((char *)p, item->valuestring, f->size);
            }
            break;
        case FIELD_U8:
            if (cJSON_IsNumber(item) && item->valuedouble >= 0 && item->valuedouble <= UINT8_MAX) {
                *p = (uint8_t)item->valuedouble;
            }
            break;
        case FIELD_U16:
            if (cJSON_IsNumber(item) && item->valuedouble >= 0 && item->valuedouble <= UINT16_MAX) {
                const uint16_t v = (uint16_t)item->valuedouble;
                memcpy(p, &v, sizeof(v));
            }
            break;
        case FIELD_BOOL:
            if (cJSON_IsBool(item)) {
                *(bool *)p = cJSON_IsTrue(item);
            }
            break;
        }
    }
}

static esp_err_t handle_post_config(httpd_req_t *req)
{
    cJSON *root = read_json(req);
    if (root == NULL) {
        return ESP_OK;
    }
    app_config_t *cfg = app_config_alloc();
    if (cfg == NULL) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    app_config_get(cfg);
    apply_fields(root, cfg);
    cJSON_Delete(root);
    esp_err_t err = app_config_set(cfg);
    free(cfg);
    return send_result(req, err == ESP_OK, err == ESP_OK ? NULL : "could not save settings");
}

static esp_err_t handle_scan(httpd_req_t *req)
{
    wifi_mgr_ap_record_t *aps = calloc(SCAN_MAX, sizeof(*aps));
    cJSON *list = cJSON_CreateArray();
    if (aps == NULL || list == NULL) {
        free(aps);
        cJSON_Delete(list);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    size_t count = 0;
    if (wifi_mgr_scan(aps, SCAN_MAX, &count) == ESP_OK) {
        for (size_t i = 0; i < count; i++) {
            cJSON *ap = cJSON_CreateObject();
            if (ap == NULL) {
                break;
            }
            cJSON_AddStringToObject(ap, "ssid", aps[i].ssid);
            cJSON_AddNumberToObject(ap, "rssi", aps[i].rssi);
            cJSON_AddBoolToObject(ap, "secure", aps[i].secure);
            cJSON_AddItemToArray(list, ap);
        }
    }
    free(aps);
    return send_json(req, list);
}

static esp_err_t handle_wifi(httpd_req_t *req)
{
    cJSON *root = read_json(req);
    if (root == NULL) {
        return ESP_OK;
    }
    const char *ssid = json_str(root, "ssid");
    const char *pass = json_str(root, "password");
    if (ssid == NULL || ssid[0] == '\0' || strlen(ssid) >= APP_CONFIG_SSID_LEN ||
            (pass != NULL && strlen(pass) >= APP_CONFIG_PASS_LEN)) {
        cJSON_Delete(root);
        return send_result(req, false, "invalid network name or password");
    }

    app_config_t *cfg = app_config_alloc();
    esp_err_t err = ESP_ERR_NO_MEM;
    if (cfg != NULL) {
        app_config_get(cfg);
        strlcpy(cfg->wifi_ssid, ssid, sizeof(cfg->wifi_ssid));
        strlcpy(cfg->wifi_pass, pass != NULL ? pass : "", sizeof(cfg->wifi_pass));
        err = app_config_set(cfg);
        free(cfg);
    }
    if (err == ESP_OK) {
        err = wifi_mgr_connect(ssid, pass);
    }
    cJSON_Delete(root);
    return send_result(req, err == ESP_OK, err == ESP_OK ? NULL : esp_err_to_name(err));
}

/* List models using the posted settings (falling back to stored ones). */
static esp_err_t handle_test(httpd_req_t *req)
{
    cJSON *root = read_json(req);
    if (root == NULL) {
        return ESP_OK;
    }
    app_config_t *cfg = app_config_alloc();
    char *models = heap_caps_malloc(MODELS_JSON_MAX, MALLOC_CAP_SPIRAM);
    ai_client_handle_t ai = NULL;
    const ai_client_config_t ai_cfg = AI_CLIENT_DEFAULT_CONFIG();
    esp_err_t err = ESP_ERR_NO_MEM;
    cJSON *result = cJSON_CreateObject();

    if (cfg != NULL && models != NULL && result != NULL &&
            ai_client_create(&ai_cfg, &ai) == ESP_OK) {
        app_config_get(cfg);
        apply_fields(root, cfg);
        const ai_endpoint_t ep = { .base_url = cfg->api_base_url, .api_key = cfg->api_key };
        err = ai_client_list_models(ai, &ep, models, MODELS_JSON_MAX);
        cJSON_AddBoolToObject(result, "ok", err == ESP_OK);
        if (err == ESP_OK) {
            cJSON_AddItemToObject(result, "models", cJSON_Parse(models));
        } else {
            cJSON_AddStringToObject(result, "error", ai_client_last_error(ai));
        }
    }
    ai_client_delete(ai);
    free(models);
    free(cfg);
    cJSON_Delete(root);
    if (result == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    if (!cJSON_HasObjectItem(result, "ok")) {
        cJSON_AddBoolToObject(result, "ok", false);
        cJSON_AddStringToObject(result, "error", esp_err_to_name(err));
    }
    return send_json(req, result);
}

static esp_err_t handle_chat(httpd_req_t *req)
{
    cJSON *root = read_json(req);
    if (root == NULL) {
        return ESP_OK;
    }
    const char *text = json_str(root, "text");
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (text != NULL && text[0] != '\0' && s_web.config.send_chat != NULL) {
        err = s_web.config.send_chat(text, s_web.config.user_ctx);
    }
    cJSON_Delete(root);
    return send_result(req, err == ESP_OK,
                       err == ESP_OK ? NULL :
                       err == ESP_ERR_INVALID_STATE ? "Budgie is busy, try again in a moment" :
                       "message missing");
}

static esp_err_t handle_action(httpd_req_t *req)
{
    cJSON *root = read_json(req);
    if (root == NULL) {
        return ESP_OK;
    }
    static const struct {
        const char *name;
        web_config_action_t action;
    } actions[] = {
        { "reboot", WEB_ACTION_REBOOT },
        { "vibe_mode", WEB_ACTION_VIBE_MODE },
        { "clear_history", WEB_ACTION_CLEAR_HISTORY },
        { "factory_reset", WEB_ACTION_FACTORY_RESET },
    };
    const char *name = json_str(root, "action");
    int found = -1;
    for (size_t i = 0; name != NULL && i < sizeof(actions) / sizeof(actions[0]); i++) {
        if (strcmp(name, actions[i].name) == 0) {
            found = (int)i;
        }
    }
    cJSON_Delete(root);
    if (found < 0 || s_web.config.run_action == NULL) {
        return send_result(req, false, "unknown action");
    }
    /* Reply first: reboot-type actions restart the device shortly after */
    esp_err_t err = send_result(req, true, NULL);
    s_web.config.run_action(actions[found].action, s_web.config.user_ctx);
    return err;
}

static esp_err_t handle_not_found(httpd_req_t *req, httpd_err_code_t code)
{
    /* Captive portal: send every unknown page to the settings UI */
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

/* ---------- Lifecycle ---------- */

esp_err_t web_config_start(const web_config_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(s_web.server == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");
    s_web.config = *config;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = HTTPD_STACK;
    cfg.max_open_sockets = HTTPD_MAX_SOCKETS;
    cfg.max_uri_handlers = HTTPD_MAX_HANDLERS;
    cfg.lru_purge_enable = true;
    ESP_RETURN_ON_ERROR(httpd_start(&s_web.server, &cfg), TAG, "httpd_start");

    const httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = handle_index },
        { .uri = "/api/status", .method = HTTP_GET, .handler = handle_status },
        { .uri = "/api/config", .method = HTTP_GET, .handler = handle_get_config },
        { .uri = "/api/config", .method = HTTP_POST, .handler = handle_post_config },
        { .uri = "/api/wifi/scan", .method = HTTP_GET, .handler = handle_scan },
        { .uri = "/api/wifi", .method = HTTP_POST, .handler = handle_wifi },
        { .uri = "/api/test", .method = HTTP_POST, .handler = handle_test },
        { .uri = "/api/chat", .method = HTTP_POST, .handler = handle_chat },
        { .uri = "/api/action", .method = HTTP_POST, .handler = handle_action },
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        esp_err_t err = httpd_register_uri_handler(s_web.server, &uris[i]);
        if (err != ESP_OK) {
            web_config_stop();
            ESP_LOGE(TAG, "register %s: %s", uris[i].uri, esp_err_to_name(err));
            return err;
        }
    }
    httpd_register_err_handler(s_web.server, HTTPD_404_NOT_FOUND, handle_not_found);
    ESP_LOGI(TAG, "web config on port %d", cfg.server_port);
    return ESP_OK;
}

void web_config_stop(void)
{
    if (s_web.server != NULL) {
        httpd_stop(s_web.server);
        s_web.server = NULL;
    }
}
