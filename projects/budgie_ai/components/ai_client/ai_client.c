// SPDX-License-Identifier: Apache-2.0

#include "ai_client.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "ai_client";

#define URL_MAX              256
#define AUTH_HEADER_MAX      256
#define ERROR_TEXT_MAX       160
#define HTTP_RX_BUFFER       4096
#define HTTP_TX_BUFFER       2048
#define UPLOAD_CHUNK         4096
#define WAV_HEADER_SIZE      44
#define WAV_PROBE_MAX        512      /* bytes searched for the "data" chunk */
#define SPEECH_CHUNK_BYTES   4096
#define RAW_PCM_SAMPLE_RATE  24000    /* OpenAI "pcm" response format */
#define MULTIPART_BOUNDARY   "----BudgieFormBoundary7MA4YWxkTrZu0gW"
#define MULTIPART_HEAD_MAX   768

struct ai_client_t {
    ai_client_config_t config;
    char *response;              /* JSON response buffer, response_buf_size bytes */
    uint8_t *chunk;              /* streaming / upload scratch, SPEECH_CHUNK_BYTES */
    char url[URL_MAX];
    char auth[AUTH_HEADER_MAX];
    char error[ERROR_TEXT_MAX];
};

static void *alloc_psram(size_t size)
{
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p != NULL ? p : malloc(size);
}

static void set_error(ai_client_handle_t h, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(h->error, sizeof(h->error), fmt, args);
    va_end(args);
    ESP_LOGW(TAG, "%s", h->error);
}

esp_err_t ai_client_create(const ai_client_config_t *config, ai_client_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config != NULL && ret_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "bad args");
    ESP_RETURN_ON_FALSE(config->response_buf_size >= 1024, ESP_ERR_INVALID_ARG, TAG, "buffer too small");

    ai_client_handle_t h = calloc(1, sizeof(*h));
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    h->config = *config;
    h->response = alloc_psram(config->response_buf_size);
    h->chunk = alloc_psram(SPEECH_CHUNK_BYTES);
    if (h->response == NULL || h->chunk == NULL) {
        ai_client_delete(h);
        return ESP_ERR_NO_MEM;
    }
    *ret_handle = h;
    return ESP_OK;
}

void ai_client_delete(ai_client_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    free(handle->response);
    free(handle->chunk);
    free(handle);
}

const char *ai_client_last_error(ai_client_handle_t handle)
{
    return handle != NULL ? handle->error : "";
}

/* ---------- HTTP helpers ---------- */

static esp_err_t build_url(ai_client_handle_t h, const ai_endpoint_t *ep, const char *path)
{
    if (ep == NULL || ep->base_url == NULL || ep->base_url[0] == '\0') {
        set_error(h, "No API base URL configured");
        return ESP_ERR_INVALID_ARG;
    }
    int n = snprintf(h->url, sizeof(h->url), "%s%s", ep->base_url, path);
    if (n < 0 || (size_t)n >= sizeof(h->url)) {
        set_error(h, "API URL too long");
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static esp_http_client_handle_t open_request(ai_client_handle_t h, const ai_endpoint_t *ep,
                                             esp_http_client_method_t method,
                                             const char *content_type)
{
    const esp_http_client_config_t cfg = {
        .url = h->url,
        .method = method,
        .timeout_ms = h->config.timeout_ms,
        .buffer_size = HTTP_RX_BUFFER,
        .buffer_size_tx = HTTP_TX_BUFFER,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        set_error(h, "HTTP client init failed");
        return NULL;
    }
    if (content_type != NULL) {
        esp_http_client_set_header(client, "Content-Type", content_type);
    }
    if (ep->api_key != NULL && ep->api_key[0] != '\0') {
        snprintf(h->auth, sizeof(h->auth), "Bearer %s", ep->api_key);
        esp_http_client_set_header(client, "Authorization", h->auth);
    }
    return client;
}

static esp_err_t write_all(ai_client_handle_t h, esp_http_client_handle_t client,
                           const void *data, size_t len)
{
    const char *p = data;
    while (len > 0) {
        int w = esp_http_client_write(client, p, (int)len);
        if (w <= 0) {
            set_error(h, "Upload failed (%d)", w);
            return ESP_FAIL;
        }
        p += w;
        len -= (size_t)w;
    }
    return ESP_OK;
}

/* Read the whole (possibly chunked) body into h->response, NUL-terminated. */
static esp_err_t read_body(ai_client_handle_t h, esp_http_client_handle_t client, size_t *out_len)
{
    size_t total = 0;
    const size_t cap = h->config.response_buf_size - 1;
    while (true) {
        int r = esp_http_client_read(client, h->response + total, (int)(cap - total));
        if (r < 0) {
            set_error(h, "Network read failed (%d)", r);
            return ESP_FAIL;
        }
        if (r == 0) {
            break;
        }
        total += (size_t)r;
        if (total >= cap) {
            set_error(h, "Response larger than %u bytes", (unsigned)cap);
            return ESP_ERR_NO_MEM;
        }
    }
    h->response[total] = '\0';
    if (out_len != NULL) {
        *out_len = total;
    }
    return ESP_OK;
}

/* Turn a non-2xx response into a readable error using {"error":{"message":...}} */
static void set_http_error(ai_client_handle_t h, int status)
{
    const char *detail = NULL;
    cJSON *root = cJSON_Parse(h->response);
    if (root != NULL) {
        cJSON *err = cJSON_GetObjectItem(root, "error");
        cJSON *msg = cJSON_IsObject(err) ? cJSON_GetObjectItem(err, "message")
                     : (cJSON_IsString(err) ? err : NULL);
        if (cJSON_IsString(msg)) {
            detail = msg->valuestring;
        }
    }
    if (detail == NULL && h->response[0] != '\0') {
        detail = h->response;
    }
    set_error(h, "HTTP %d: %.120s", status, detail != NULL ? detail : "request failed");
    cJSON_Delete(root);
}

/* Send a complete JSON body and read the JSON response into h->response. */
static esp_err_t post_json(ai_client_handle_t h, const ai_endpoint_t *ep, const char *path,
                           const char *body)
{
    ESP_RETURN_ON_ERROR(build_url(h, ep, path), TAG, "url");
    esp_http_client_handle_t client = open_request(h, ep, HTTP_METHOD_POST, "application/json");
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = ESP_FAIL;
    const size_t len = strlen(body);
    if (esp_http_client_open(client, (int)len) != ESP_OK) {
        set_error(h, "Cannot reach %s", h->url);
        goto out;
    }
    if (write_all(h, client, body, len) != ESP_OK) {
        goto out;
    }
    if (esp_http_client_fetch_headers(client) < 0) {
        set_error(h, "No response from server");
        goto out;
    }
    const int status = esp_http_client_get_status_code(client);
    err = read_body(h, client, NULL);
    if (err == ESP_OK && (status < 200 || status >= 300)) {
        set_http_error(h, status);
        err = ESP_FAIL;
    }
out:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

/* ---------- Chat ---------- */

static const char *role_name(ai_role_t role)
{
    switch (role) {
    case AI_ROLE_SYSTEM:
        return "system";
    case AI_ROLE_ASSISTANT:
        return "assistant";
    case AI_ROLE_USER:
    default:
        return "user";
    }
}

static const char s_b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* "data:image/jpeg;base64,<...>" as a heap string (PSRAM preferred), or NULL */
static char *jpeg_data_url(const uint8_t *data, size_t len)
{
    static const char prefix[] = "data:image/jpeg;base64,";
    const size_t out_len = sizeof(prefix) - 1 + 4 * ((len + 2) / 3);
    char *out = alloc_psram(out_len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, prefix, sizeof(prefix) - 1);
    char *p = out + sizeof(prefix) - 1;
    for (size_t i = 0; i < len; i += 3) {
        const uint32_t n = ((uint32_t)data[i] << 16) |
                           ((i + 1 < len ? (uint32_t)data[i + 1] : 0) << 8) |
                           (i + 2 < len ? (uint32_t)data[i + 2] : 0);
        *p++ = s_b64[(n >> 18) & 63];
        *p++ = s_b64[(n >> 12) & 63];
        *p++ = i + 1 < len ? s_b64[(n >> 6) & 63] : '=';
        *p++ = i + 2 < len ? s_b64[n & 63] : '=';
    }
    *p = '\0';
    return out;
}

/* content: plain string, or [text, image_url] parts when a photo is attached */
static bool add_content(cJSON *msg, const ai_message_t *m)
{
    const char *text = m->content != NULL ? m->content : "";
    if (m->image_jpeg == NULL || m->image_len == 0) {
        return cJSON_AddStringToObject(msg, "content", text) != NULL;
    }
    cJSON *parts = cJSON_AddArrayToObject(msg, "content");
    cJSON *text_part = cJSON_CreateObject();
    cJSON *image_part = cJSON_CreateObject();
    char *url = jpeg_data_url(m->image_jpeg, m->image_len);
    bool ok = parts != NULL && text_part != NULL && image_part != NULL && url != NULL;
    if (ok) {
        cJSON_AddStringToObject(text_part, "type", "text");
        cJSON_AddStringToObject(text_part, "text", text);
        cJSON_AddItemToArray(parts, text_part);
        text_part = NULL;
        cJSON_AddStringToObject(image_part, "type", "image_url");
        cJSON *image_url = cJSON_AddObjectToObject(image_part, "image_url");
        ok = image_url != NULL && cJSON_AddStringToObject(image_url, "url", url) != NULL;
        cJSON_AddItemToArray(parts, image_part);
        image_part = NULL;
    }
    cJSON_Delete(text_part);
    cJSON_Delete(image_part);
    free(url);
    return ok;
}

static char *build_chat_body(const ai_chat_params_t *p, const ai_message_t *messages, size_t count)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *list = cJSON_AddArrayToObject(root, "messages");
    if (root == NULL || list == NULL) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddStringToObject(root, "model", p->endpoint.model != NULL ? p->endpoint.model : "");
    cJSON_AddNumberToObject(root, "max_tokens", p->max_tokens);
    cJSON_AddNumberToObject(root, "temperature", p->temperature);
    cJSON_AddBoolToObject(root, "stream", false);
    for (size_t i = 0; i < count; i++) {
        cJSON *m = cJSON_CreateObject();
        if (m == NULL) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddStringToObject(m, "role", role_name(messages[i].role));
        cJSON_AddItemToArray(list, m);
        if (!add_content(m, &messages[i])) {
            cJSON_Delete(root);
            return NULL;
        }
    }
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return body;
}

esp_err_t ai_client_chat(ai_client_handle_t h, const ai_chat_params_t *params,
                         const ai_message_t *messages, size_t message_count,
                         char *reply, size_t reply_size)
{
    ESP_RETURN_ON_FALSE(h != NULL && params != NULL && messages != NULL && reply != NULL &&
                        reply_size > 0, ESP_ERR_INVALID_ARG, TAG, "bad args");
    h->error[0] = '\0';
    reply[0] = '\0';

    char *body = build_chat_body(params, messages, message_count);
    if (body == NULL) {
        set_error(h, "Out of memory building request");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = post_json(h, &params->endpoint, "/chat/completions", body);
    cJSON_free(body);
    if (err != ESP_OK) {
        return err;
    }

    cJSON *root = cJSON_Parse(h->response);
    cJSON *choices = root != NULL ? cJSON_GetObjectItem(root, "choices") : NULL;
    cJSON *first = cJSON_IsArray(choices) ? cJSON_GetArrayItem(choices, 0) : NULL;
    cJSON *message = first != NULL ? cJSON_GetObjectItem(first, "message") : NULL;
    cJSON *content = message != NULL ? cJSON_GetObjectItem(message, "content") : NULL;
    if (cJSON_IsString(content)) {
        strlcpy(reply, content->valuestring, reply_size);
    } else {
        set_error(h, "Unexpected chat response");
        err = ESP_ERR_INVALID_RESPONSE;
    }
    cJSON_Delete(root);
    return err;
}

/* ---------- Speech to text ---------- */

static void wav_header(uint8_t *out, uint32_t data_bytes, uint32_t sample_rate)
{
    const uint32_t byte_rate = sample_rate * sizeof(int16_t);
    const uint32_t riff_size = data_bytes + WAV_HEADER_SIZE - 8;
    memcpy(out, "RIFF", 4);
    memcpy(out + 4, &riff_size, 4);
    memcpy(out + 8, "WAVEfmt ", 8);
    const uint32_t fmt_size = 16;
    const uint16_t pcm_format = 1, channels = 1, block_align = sizeof(int16_t), bits = 16;
    memcpy(out + 16, &fmt_size, 4);
    memcpy(out + 20, &pcm_format, 2);
    memcpy(out + 22, &channels, 2);
    memcpy(out + 24, &sample_rate, 4);
    memcpy(out + 28, &byte_rate, 4);
    memcpy(out + 32, &block_align, 2);
    memcpy(out + 34, &bits, 2);
    memcpy(out + 36, "data", 4);
    memcpy(out + 40, &data_bytes, 4);
}

esp_err_t ai_client_transcribe(ai_client_handle_t h, const ai_endpoint_t *endpoint,
                               const char *language, const int16_t *pcm, size_t samples,
                               uint32_t sample_rate, char *text, size_t text_size)
{
    ESP_RETURN_ON_FALSE(h != NULL && endpoint != NULL && pcm != NULL && samples > 0 &&
                        text != NULL && text_size > 0, ESP_ERR_INVALID_ARG, TAG, "bad args");
    h->error[0] = '\0';
    text[0] = '\0';
    ESP_RETURN_ON_ERROR(build_url(h, endpoint, "/audio/transcriptions"), TAG, "url");

    /* Multipart preamble is built in the chunk scratch buffer (never on the stack) */
    char *head = (char *)h->chunk;
    int head_len = snprintf(head, MULTIPART_HEAD_MAX,
        "--" MULTIPART_BOUNDARY "\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n%s\r\n"
        "--" MULTIPART_BOUNDARY "\r\n"
        "Content-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n"
        "%s%s%s"
        "--" MULTIPART_BOUNDARY "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n",
        endpoint->model != NULL ? endpoint->model : "",
        (language != NULL && language[0]) ? "--" MULTIPART_BOUNDARY "\r\n"
            "Content-Disposition: form-data; name=\"language\"\r\n\r\n" : "",
        (language != NULL && language[0]) ? language : "",
        (language != NULL && language[0]) ? "\r\n" : "");
    if (head_len < 0 || head_len >= MULTIPART_HEAD_MAX) {
        set_error(h, "Request header too long");
        return ESP_ERR_INVALID_SIZE;
    }
    static const char tail[] = "\r\n--" MULTIPART_BOUNDARY "--\r\n";
    uint8_t *wav = h->chunk + MULTIPART_HEAD_MAX;
    const uint32_t data_bytes = (uint32_t)(samples * sizeof(int16_t));
    wav_header(wav, data_bytes, sample_rate);

    esp_http_client_handle_t client = open_request(h, endpoint, HTTP_METHOD_POST,
                                                   "multipart/form-data; boundary=" MULTIPART_BOUNDARY);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_err_t err = ESP_FAIL;
    const size_t total = (size_t)head_len + WAV_HEADER_SIZE + data_bytes + (sizeof(tail) - 1);
    if (esp_http_client_open(client, (int)total) != ESP_OK) {
        set_error(h, "Cannot reach %s", h->url);
        goto out;
    }
    if (write_all(h, client, head, (size_t)head_len) != ESP_OK ||
        write_all(h, client, wav, WAV_HEADER_SIZE) != ESP_OK) {
        goto out;
    }
    const uint8_t *data = (const uint8_t *)pcm;
    for (size_t off = 0; off < data_bytes; off += UPLOAD_CHUNK) {
        const size_t n = (data_bytes - off) < UPLOAD_CHUNK ? (data_bytes - off) : UPLOAD_CHUNK;
        if (write_all(h, client, data + off, n) != ESP_OK) {
            goto out;
        }
    }
    if (write_all(h, client, tail, sizeof(tail) - 1) != ESP_OK) {
        goto out;
    }
    if (esp_http_client_fetch_headers(client) < 0) {
        set_error(h, "No response from server");
        goto out;
    }
    const int status = esp_http_client_get_status_code(client);
    err = read_body(h, client, NULL);
    if (err == ESP_OK && (status < 200 || status >= 300)) {
        set_http_error(h, status);
        err = ESP_FAIL;
    }
out:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) {
        return err;
    }

    cJSON *root = cJSON_Parse(h->response);
    cJSON *t = root != NULL ? cJSON_GetObjectItem(root, "text") : NULL;
    if (cJSON_IsString(t)) {
        strlcpy(text, t->valuestring, text_size);
    } else {
        set_error(h, "Unexpected transcription response");
        err = ESP_ERR_INVALID_RESPONSE;
    }
    cJSON_Delete(root);
    return err;
}

/* ---------- Text to speech ---------- */

typedef struct {
    bool header_done;
    uint16_t channels;
    uint32_t sample_rate;
    size_t pending;              /* unconsumed bytes at the start of the chunk buffer */
} speech_stream_t;

/* Parse RIFF chunks in the first bytes. Returns the data offset, 0 if more
 * bytes are needed, or -1 when the stream is not WAV (raw PCM). */
static int parse_wav(speech_stream_t *s, const uint8_t *buf, size_t len)
{
    if (len < 12) {
        return 0;
    }
    if (memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
        return -1;
    }
    size_t pos = 12;
    while (pos + 8 <= len) {
        uint32_t size;
        memcpy(&size, buf + pos + 4, 4);
        if (memcmp(buf + pos, "fmt ", 4) == 0 && pos + 8 + 16 <= len) {
            memcpy(&s->channels, buf + pos + 10, 2);
            memcpy(&s->sample_rate, buf + pos + 12, 4);
        } else if (memcmp(buf + pos, "data", 4) == 0) {
            return (int)(pos + 8);
        }
        if (size > WAV_PROBE_MAX) {
            return -1;
        }
        pos += 8 + size + (size & 1);
    }
    return 0;
}

/* Consume the pending bytes in @p buf: parse the header once, then hand every
 * complete frame (downmixed to mono) to the sink and keep the partial tail. */
static esp_err_t drain_pcm(speech_stream_t *s, uint8_t *buf, bool final,
                           ai_pcm_sink_t sink, void *ctx)
{
    if (!s->header_done) {
        int off = parse_wav(s, buf, s->pending);
        if (off == 0 && s->pending < WAV_PROBE_MAX && !final) {
            return ESP_OK;  /* need more header bytes */
        }
        if (off <= 0) {
            off = 0;        /* raw PCM (or an unparsable header): play everything */
            s->channels = 1;
            s->sample_rate = RAW_PCM_SAMPLE_RATE;
        }
        ESP_RETURN_ON_FALSE(s->channels == 1 || s->channels == 2, ESP_ERR_NOT_SUPPORTED,
                            TAG, "unsupported channel count %u", s->channels);
        s->pending -= (size_t)off;
        memmove(buf, buf + off, s->pending);   /* keep samples 2-byte aligned */
        s->header_done = true;
    }

    const size_t frame = sizeof(int16_t) * s->channels;
    const size_t frames = s->pending / frame;
    const size_t used = frames * frame;
    int16_t *pcm = (int16_t *)buf;
    if (s->channels == 2) {
        for (size_t i = 0; i < frames; i++) {
            pcm[i] = (int16_t)(((int32_t)pcm[2 * i] + pcm[2 * i + 1]) / 2);
        }
    }
    esp_err_t err = frames > 0 ? sink(pcm, frames, s->sample_rate, ctx) : ESP_OK;
    s->pending -= used;
    memmove(buf, buf + used, s->pending);
    return err;
}

esp_err_t ai_client_speech(ai_client_handle_t h, const ai_endpoint_t *endpoint,
                           const char *voice, const char *text,
                           ai_pcm_sink_t sink, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(h != NULL && endpoint != NULL && text != NULL && sink != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "bad args");
    h->error[0] = '\0';
    ESP_RETURN_ON_ERROR(build_url(h, endpoint, "/audio/speech"), TAG, "url");

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "model", endpoint->model != NULL ? endpoint->model : "");
    cJSON_AddStringToObject(root, "input", text);
    cJSON_AddStringToObject(root, "voice", voice != NULL ? voice : "alloy");
    cJSON_AddStringToObject(root, "response_format", "wav");
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (body == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_FAIL;
    esp_http_client_handle_t client = open_request(h, endpoint, HTTP_METHOD_POST, "application/json");
    if (client == NULL) {
        cJSON_free(body);
        return ESP_FAIL;
    }
    const size_t body_len = strlen(body);
    if (esp_http_client_open(client, (int)body_len) != ESP_OK) {
        set_error(h, "Cannot reach %s", h->url);
        goto out;
    }
    if (write_all(h, client, body, body_len) != ESP_OK) {
        goto out;
    }
    if (esp_http_client_fetch_headers(client) < 0) {
        set_error(h, "No response from server");
        goto out;
    }
    const int status = esp_http_client_get_status_code(client);
    if (status < 200 || status >= 300) {
        if (read_body(h, client, NULL) == ESP_OK) {
            set_http_error(h, status);
        }
        goto out;
    }

    speech_stream_t s = { .channels = 1, .sample_rate = RAW_PCM_SAMPLE_RATE };
    err = ESP_OK;
    while (err == ESP_OK) {
        int r = esp_http_client_read(client, (char *)h->chunk + s.pending,
                                     (int)(SPEECH_CHUNK_BYTES - s.pending));
        if (r < 0) {
            set_error(h, "Audio stream interrupted (%d)", r);
            err = ESP_FAIL;
            break;
        }
        if (r == 0) {
            if (!s.header_done && s.pending > 0) {
                err = drain_pcm(&s, h->chunk, true, sink, user_ctx);   /* very short clip */
            }
            break;
        }
        s.pending += (size_t)r;
        err = drain_pcm(&s, h->chunk, false, sink, user_ctx);
    }
out:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    cJSON_free(body);
    return err;
}

/* ---------- Models ---------- */

esp_err_t ai_client_list_models(ai_client_handle_t h, const ai_endpoint_t *endpoint,
                                char *out, size_t out_size)
{
    ESP_RETURN_ON_FALSE(h != NULL && endpoint != NULL && out != NULL && out_size > 2,
                        ESP_ERR_INVALID_ARG, TAG, "bad args");
    h->error[0] = '\0';
    strlcpy(out, "[]", out_size);
    ESP_RETURN_ON_ERROR(build_url(h, endpoint, "/models"), TAG, "url");

    esp_http_client_handle_t client = open_request(h, endpoint, HTTP_METHOD_GET, NULL);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_err_t err = ESP_FAIL;
    if (esp_http_client_open(client, 0) != ESP_OK) {
        set_error(h, "Cannot reach %s", h->url);
        goto out;
    }
    if (esp_http_client_fetch_headers(client) < 0) {
        set_error(h, "No response from server");
        goto out;
    }
    const int status = esp_http_client_get_status_code(client);
    err = read_body(h, client, NULL);
    if (err == ESP_OK && (status < 200 || status >= 300)) {
        set_http_error(h, status);
        err = ESP_FAIL;
    }
out:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) {
        return err;
    }

    cJSON *root = cJSON_Parse(h->response);
    cJSON *data = root != NULL ? cJSON_GetObjectItem(root, "data") : NULL;
    cJSON *ids = cJSON_CreateArray();
    if (cJSON_IsArray(data) && ids != NULL) {
        cJSON *item;
        cJSON_ArrayForEach(item, data) {
            cJSON *id = cJSON_GetObjectItem(item, "id");
            if (cJSON_IsString(id)) {
                cJSON_AddItemToArray(ids, cJSON_CreateString(id->valuestring));
            }
        }
        char *printed = cJSON_PrintUnformatted(ids);
        if (printed != NULL && strlen(printed) < out_size) {
            strlcpy(out, printed, out_size);
        }
        cJSON_free(printed);
    } else {
        set_error(h, "Unexpected models response");
        err = ESP_ERR_INVALID_RESPONSE;
    }
    cJSON_Delete(ids);
    cJSON_Delete(root);
    return err;
}
