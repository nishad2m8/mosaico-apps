// SPDX-License-Identifier: Apache-2.0

#include "iss_net.h"

#include <string.h>
#include <time.h>

#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

static const char *TAG = "iss_net";

/* Vibe Mode keeps the Wi-Fi it was given here */
#define VIBE_NVS_PARTITION    "sysmeta"
#define VIBE_NVS_NAMESPACE    "wifi"
#define TLE_NVS_NAMESPACE     "iss"
#define TLE_URL               "https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=TLE"
#define SNTP_SERVER           "pool.ntp.org"
#define HTTP_TIMEOUT_MS       10000
#define TLE_BODY_MAX          512
#define TLE_REFRESH_US        (6LL * 3600 * 1000000)   /* CelesTrak updates a few times a day */
#define TLE_RETRY_US          (5LL * 60 * 1000000)
#define WORKER_STACK          6144
#define WORKER_PRIORITY       3
#define WORKER_POLL_MS        1000
#define VALID_TIME_UNIX       1735689600   /* 2025-01-01: anything earlier is an unset clock */
#define SSID_SIZE             33
#define PASSWORD_SIZE         65
#define GOT_IP_BIT            BIT0

static struct {
    SemaphoreHandle_t lock;          /* guards state and the pending TLE */
    EventGroupHandle_t events;
    iss_net_state_t state;
    bool sntp_started;
    bool tle_pending;
    char tle1[ISS_NET_TLE_LINE_SIZE];
    char tle2[ISS_NET_TLE_LINE_SIZE];
    char body[TLE_BODY_MAX];         /* worker-task only */
} s_net;

static void set_state(iss_net_state_t state)
{
    xSemaphoreTake(s_net.lock, portMAX_DELAY);
    s_net.state = state;
    xSemaphoreGive(s_net.lock);
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_net.events, GOT_IP_BIT);
        set_state(ISS_NET_CONNECTING);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_net.events, GOT_IP_BIT);
        set_state(ISS_NET_ONLINE);
        if (!s_net.sntp_started) {
            esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
            s_net.sntp_started = esp_netif_sntp_init(&sntp) == ESP_OK;
        }
    }
}

static bool read_vibe_wifi(char ssid[SSID_SIZE], char password[PASSWORD_SIZE])
{
    nvs_handle_t nvs;
    if (nvs_open_from_partition(VIBE_NVS_PARTITION, VIBE_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    size_t ssid_len = SSID_SIZE, pass_len = PASSWORD_SIZE;
    const bool ok = nvs_get_str(nvs, "ssid", ssid, &ssid_len) == ESP_OK && ssid[0] != '\0';
    if (!ok || nvs_get_str(nvs, "password", password, &pass_len) != ESP_OK) {
        password[0] = '\0';
    }
    nvs_close(nvs);
    return ok;
}

/* Copy one line of @p text starting at *pos, without CR/LF. */
static bool next_line(const char *text, size_t *pos, char *out, size_t out_size)
{
    size_t n = 0;
    while (text[*pos] == '\r' || text[*pos] == '\n') {
        ++*pos;
    }
    if (text[*pos] == '\0') {
        return false;
    }
    while (text[*pos] != '\0' && text[*pos] != '\r' && text[*pos] != '\n') {
        if (n + 1 < out_size) {
            out[n++] = text[*pos];
        }
        ++*pos;
    }
    while (n > 0 && out[n - 1] == ' ') {
        --n;
    }
    out[n] = '\0';
    return true;
}

static esp_err_t save_tle(const char *l1, const char *l2)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(TLE_NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG, "open nvs");
    esp_err_t err = nvs_set_str(nvs, "tle1", l1);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, "tle2", l2);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

static esp_err_t fetch_tle(void)
{
    const esp_http_client_config_t cfg = {
        .url = TLE_URL,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    ESP_RETURN_ON_FALSE(client != NULL, ESP_ERR_NO_MEM, TAG, "http client");
    esp_err_t err = esp_http_client_open(client, 0);
    int len = 0;
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int r;
        while ((r = esp_http_client_read(client, s_net.body + len, (int)sizeof(s_net.body) - 1 - len)) > 0) {
            len += r;
        }
        if (esp_http_client_get_status_code(client) != 200 || r < 0) {
            err = ESP_FAIL;
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    ESP_RETURN_ON_ERROR(err, TAG, "download failed");
    s_net.body[len] = '\0';

    /* "ISS (ZARYA)", line 1, line 2 */
    char name[32], l1[ISS_NET_TLE_LINE_SIZE + 8], l2[ISS_NET_TLE_LINE_SIZE + 8];
    size_t pos = 0;
    ESP_RETURN_ON_FALSE(next_line(s_net.body, &pos, name, sizeof(name)) &&
                        next_line(s_net.body, &pos, l1, sizeof(l1)) &&
                        next_line(s_net.body, &pos, l2, sizeof(l2)),
                        ESP_ERR_INVALID_RESPONSE, TAG, "short response");
    sgp4_record_t check;
    ESP_RETURN_ON_FALSE(sgp4_init(&check, l1, l2) == SGP4_OK, ESP_ERR_INVALID_RESPONSE, TAG, "bad TLE");

    if (save_tle(l1, l2) != ESP_OK) {
        ESP_LOGW(TAG, "could not save TLE; using it for this session only");
    }
    xSemaphoreTake(s_net.lock, portMAX_DELAY);
    strlcpy(s_net.tle1, l1, sizeof(s_net.tle1));
    strlcpy(s_net.tle2, l2, sizeof(s_net.tle2));
    s_net.tle_pending = true;
    xSemaphoreGive(s_net.lock);
    ESP_LOGI(TAG, "orbit data updated");
    return ESP_OK;
}

static void worker(void *arg)
{
    int64_t next_fetch_us = 0;
    for (;;) {
        const EventBits_t bits = xEventGroupWaitBits(s_net.events, GOT_IP_BIT, pdFALSE, pdTRUE,
                                                     pdMS_TO_TICKS(WORKER_POLL_MS));
        if (!(bits & GOT_IP_BIT)) {
            continue;   /* the wait above already slept */
        }
        if (esp_timer_get_time() >= next_fetch_us) {
            next_fetch_us = esp_timer_get_time() + (fetch_tle() == ESP_OK ? TLE_REFRESH_US : TLE_RETRY_US);
        }
        vTaskDelay(pdMS_TO_TICKS(WORKER_POLL_MS));
    }
}

esp_err_t iss_net_start(void)
{
    ESP_RETURN_ON_FALSE(s_net.lock == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");
    s_net.lock = xSemaphoreCreateMutex();
    s_net.events = xEventGroupCreate();
    if (!s_net.lock || !s_net.events) {
        if (s_net.lock) {
            vSemaphoreDelete(s_net.lock);
        }
        if (s_net.events) {
            vEventGroupDelete(s_net.events);
        }
        s_net.lock = NULL;
        s_net.events = NULL;
        return ESP_ERR_NO_MEM;
    }

    char ssid[SSID_SIZE] = {0}, password[PASSWORD_SIZE] = {0};
    if (!read_vibe_wifi(ssid, password)) {
        ESP_LOGW(TAG, "no Wi-Fi saved by Vibe Mode; orbit data and time stay offline");
        s_net.state = ISS_NET_NO_WIFI;
        return ESP_OK;
    }
    s_net.state = ISS_NET_CONNECTING;

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    esp_err_t err = esp_event_loop_create_default();
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "event loop");
    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_sta() != NULL, ESP_ERR_NO_MEM, TAG, "sta netif");
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "storage");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL, NULL),
                        TAG, "wifi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL, NULL),
                        TAG, "ip events");

    wifi_config_t sta = {0};
    strlcpy((char *)sta.sta.ssid, ssid, sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, password, sizeof(sta.sta.password));
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta), TAG, "config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");   /* STA_START connects */

    ESP_RETURN_ON_FALSE(xTaskCreate(worker, "iss_net", WORKER_STACK, NULL, WORKER_PRIORITY, NULL) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "worker task");
    ESP_LOGI(TAG, "joining Vibe Mode Wi-Fi \"%s\"", ssid);
    return ESP_OK;
}

iss_net_state_t iss_net_get_state(void)
{
    if (!s_net.lock) {
        return ISS_NET_NO_WIFI;
    }
    xSemaphoreTake(s_net.lock, portMAX_DELAY);
    const iss_net_state_t state = s_net.state;
    xSemaphoreGive(s_net.lock);
    return state;
}

bool iss_net_time_valid(void)
{
    return time(NULL) >= VALID_TIME_UNIX;
}

esp_err_t iss_net_load_saved_tle(char line1[ISS_NET_TLE_LINE_SIZE], char line2[ISS_NET_TLE_LINE_SIZE])
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(TLE_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : err;
    }
    size_t len1 = ISS_NET_TLE_LINE_SIZE, len2 = ISS_NET_TLE_LINE_SIZE;
    err = nvs_get_str(nvs, "tle1", line1, &len1);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs, "tle2", line2, &len2);
    }
    nvs_close(nvs);
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : err;
}

bool iss_net_take_tle(char line1[ISS_NET_TLE_LINE_SIZE], char line2[ISS_NET_TLE_LINE_SIZE])
{
    if (!s_net.lock) {
        return false;
    }
    xSemaphoreTake(s_net.lock, portMAX_DELAY);
    const bool pending = s_net.tle_pending;
    if (pending) {
        strlcpy(line1, s_net.tle1, ISS_NET_TLE_LINE_SIZE);
        strlcpy(line2, s_net.tle2, ISS_NET_TLE_LINE_SIZE);
        s_net.tle_pending = false;
    }
    xSemaphoreGive(s_net.lock);
    return pending;
}
