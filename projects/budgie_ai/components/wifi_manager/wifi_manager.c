// SPDX-License-Identifier: Apache-2.0
//
// State machine:
//   start(ssid)  -> CONNECTING --got IP--> CONNECTED (setup AP closed after a grace period)
//                    |  disconnect: retry up to max_retries
//                    v
//                  FAILED: setup AP + captive DNS; station retried periodically
//                          while no phone is connected to the setup AP
//   start("")    -> IDLE: setup AP only

#include "wifi_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dns_captive.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "wifi_mgr";

#define AP_CHANNEL               1
#define AP_MAX_CONNECTIONS       4
#define AP_CLOSE_DELAY_US        (15 * 1000 * 1000)   /* lets the web page show success */
#define STA_RETRY_PERIOD_US      (60 * 1000 * 1000)   /* background retry while FAILED */
#define RSSI_LEVEL_3_DBM         (-60)
#define RSSI_LEVEL_2_DBM         (-70)
#define SCAN_MAX_RECORDS         24

static struct {
    SemaphoreHandle_t lock;
    wifi_mgr_config_t config;
    wifi_mgr_status_t status;
    char password[65];
    uint8_t retries;
    uint8_t ap_clients;
    bool scanning;
    esp_netif_t *sta_netif;
    esp_netif_t *ap_netif;
    esp_timer_handle_t ap_close_timer;
    esp_timer_handle_t retry_timer;
} s_wifi;

static void notify(void)
{
    wifi_mgr_status_t copy;
    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    copy = s_wifi.status;
    xSemaphoreGive(s_wifi.lock);
    if (s_wifi.config.status_cb != NULL) {
        s_wifi.config.status_cb(&copy, s_wifi.config.user_ctx);
    }
}

static void set_state(wifi_mgr_state_t state)
{
    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    s_wifi.status.state = state;
    if (state != WIFI_MGR_STATE_CONNECTED) {
        s_wifi.status.ip[0] = '\0';
        s_wifi.status.rssi = 0;
    }
    xSemaphoreGive(s_wifi.lock);
    notify();
}

/* ---------- Setup AP ---------- */

static esp_err_t ap_enable(void)
{
    if (s_wifi.status.ap_active) {
        return ESP_OK;
    }
    wifi_config_t ap = {
        .ap = {
            .channel = AP_CHANNEL,
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    const char *pass = s_wifi.config.ap_password;
    if (pass != NULL && strlen(pass) >= 8) {
        strlcpy((char *)ap.ap.password, pass, sizeof(ap.ap.password));
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }
    strlcpy((char *)ap.ap.ssid, s_wifi.status.ap_ssid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = (uint8_t)strlen(s_wifi.status.ap_ssid);

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "set APSTA");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap), TAG, "AP config");

    esp_netif_ip_info_t ip;
    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(s_wifi.ap_netif, &ip), TAG, "AP ip");
    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    s_wifi.status.ap_active = true;
    snprintf(s_wifi.status.ap_ip, sizeof(s_wifi.status.ap_ip), IPSTR, IP2STR(&ip.ip));
    xSemaphoreGive(s_wifi.lock);

    esp_err_t err = dns_captive_start(ip.ip.addr);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "captive DNS unavailable: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "setup AP \"%s\" at %s", s_wifi.status.ap_ssid, s_wifi.status.ap_ip);
    return ESP_OK;
}

static void ap_disable(void)
{
    if (!s_wifi.status.ap_active) {
        return;
    }
    dns_captive_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    s_wifi.status.ap_active = false;
    s_wifi.status.ap_ip[0] = '\0';
    s_wifi.ap_clients = 0;
    xSemaphoreGive(s_wifi.lock);
    ESP_LOGI(TAG, "setup AP closed");
}

static void ap_close_timer_cb(void *arg)
{
    if (s_wifi.status.state == WIFI_MGR_STATE_CONNECTED) {
        ap_disable();
        notify();
    }
}

/* ---------- Station ---------- */

static esp_err_t sta_connect(void)
{
    wifi_config_t sta = { 0 };
    strlcpy((char *)sta.sta.ssid, s_wifi.status.ssid, sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, s_wifi.password, sizeof(sta.sta.password));
    sta.sta.threshold.authmode = s_wifi.password[0] ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;

    s_wifi.retries = 0;
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta), TAG, "STA config");
    set_state(WIFI_MGR_STATE_CONNECTING);
    esp_wifi_disconnect();
    return esp_wifi_connect();
}

static void retry_timer_cb(void *arg)
{
    /* Retrying hops channels, which would drop phones using the setup page */
    if (s_wifi.status.state == WIFI_MGR_STATE_FAILED && s_wifi.ap_clients == 0 &&
            s_wifi.status.ssid[0] != '\0' && !s_wifi.scanning) {
        ESP_LOGI(TAG, "retrying \"%s\"", s_wifi.status.ssid);
        sta_connect();
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case WIFI_EVENT_STA_START:
        if (s_wifi.status.ssid[0] != '\0') {
            sta_connect();
        }
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        if (s_wifi.scanning || s_wifi.status.ssid[0] == '\0') {
            break;
        }
        const wifi_event_sta_disconnected_t *d = data;
        if (d != NULL && d->reason == WIFI_REASON_ASSOC_LEAVE) {
            break;   /* our own esp_wifi_disconnect() before switching networks */
        }
        ESP_LOGW(TAG, "disconnected (reason %d)", d != NULL ? d->reason : -1);
        if (s_wifi.retries < s_wifi.config.max_retries) {
            s_wifi.retries++;
            set_state(WIFI_MGR_STATE_CONNECTING);
            esp_wifi_connect();
        } else if (s_wifi.status.state != WIFI_MGR_STATE_FAILED) {
            ap_enable();
            set_state(WIFI_MGR_STATE_FAILED);
        }
        break;
    }
    case WIFI_EVENT_AP_STACONNECTED:
        s_wifi.ap_clients++;
        break;
    case WIFI_EVENT_AP_STADISCONNECTED:
        if (s_wifi.ap_clients > 0) {
            s_wifi.ap_clients--;
        }
        break;
    default:
        break;
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id != IP_EVENT_STA_GOT_IP) {
        return;
    }
    const ip_event_got_ip_t *e = data;
    wifi_ap_record_t info;
    int8_t rssi = esp_wifi_sta_get_ap_info(&info) == ESP_OK ? info.rssi : 0;

    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    snprintf(s_wifi.status.ip, sizeof(s_wifi.status.ip), IPSTR, IP2STR(&e->ip_info.ip));
    s_wifi.status.rssi = rssi;
    s_wifi.status.state = WIFI_MGR_STATE_CONNECTED;
    xSemaphoreGive(s_wifi.lock);
    s_wifi.retries = 0;
    ESP_LOGI(TAG, "connected to \"%s\", ip %s", s_wifi.status.ssid, s_wifi.status.ip);

    if (s_wifi.status.ap_active) {
        esp_timer_stop(s_wifi.ap_close_timer);
        esp_timer_start_once(s_wifi.ap_close_timer, AP_CLOSE_DELAY_US);
    }
    notify();
}

/* ---------- Public API ---------- */

esp_err_t wifi_mgr_start(const wifi_mgr_config_t *config, const char *ssid, const char *password)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    ESP_RETURN_ON_FALSE(s_wifi.lock == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");

    s_wifi.lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_wifi.lock != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    s_wifi.config = *config;

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    esp_err_t err = esp_event_loop_create_default();
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "event loop");

    s_wifi.sta_netif = esp_netif_create_default_wifi_sta();
    s_wifi.ap_netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(s_wifi.sta_netif && s_wifi.ap_netif, ESP_ERR_NO_MEM, TAG, "netifs");

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "storage");

    const esp_timer_create_args_t close_args = { .callback = ap_close_timer_cb, .name = "wifi_ap_close" };
    const esp_timer_create_args_t retry_args = { .callback = retry_timer_cb, .name = "wifi_retry" };
    ESP_RETURN_ON_ERROR(esp_timer_create(&close_args, &s_wifi.ap_close_timer), TAG, "timer");
    ESP_RETURN_ON_ERROR(esp_timer_create(&retry_args, &s_wifi.retry_timer), TAG, "timer");

    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                        on_wifi_event, NULL, NULL), TAG, "wifi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                        on_ip_event, NULL, NULL), TAG, "ip events");

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_wifi.status.ap_ssid, sizeof(s_wifi.status.ap_ssid), "%s-%02X%02X",
             config->ap_ssid_prefix != NULL ? config->ap_ssid_prefix : "Setup", mac[4], mac[5]);
    if (ssid != NULL) {
        strlcpy(s_wifi.status.ssid, ssid, sizeof(s_wifi.status.ssid));
    }
    if (password != NULL) {
        strlcpy(s_wifi.password, password, sizeof(s_wifi.password));
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");   /* STA_START triggers connect */
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_wifi.retry_timer, STA_RETRY_PERIOD_US), TAG, "retry");

    if (s_wifi.status.ssid[0] == '\0') {
        ESP_RETURN_ON_ERROR(ap_enable(), TAG, "setup AP");
        set_state(WIFI_MGR_STATE_IDLE);
    }
    return ESP_OK;
}

esp_err_t wifi_mgr_connect(const char *ssid, const char *password)
{
    ESP_RETURN_ON_FALSE(s_wifi.lock != NULL, ESP_ERR_INVALID_STATE, TAG, "not started");
    ESP_RETURN_ON_FALSE(ssid != NULL && ssid[0] != '\0' && strlen(ssid) < WIFI_MGR_SSID_LEN,
                        ESP_ERR_INVALID_ARG, TAG, "bad ssid");
    ESP_RETURN_ON_FALSE(password == NULL || strlen(password) < sizeof(s_wifi.password),
                        ESP_ERR_INVALID_ARG, TAG, "bad password");

    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    strlcpy(s_wifi.status.ssid, ssid, sizeof(s_wifi.status.ssid));
    strlcpy(s_wifi.password, password != NULL ? password : "", sizeof(s_wifi.password));
    xSemaphoreGive(s_wifi.lock);
    return sta_connect();
}

esp_err_t wifi_mgr_open_setup_ap(void)
{
    ESP_RETURN_ON_FALSE(s_wifi.lock != NULL, ESP_ERR_INVALID_STATE, TAG, "not started");
    esp_timer_stop(s_wifi.ap_close_timer);
    esp_err_t err = ap_enable();
    notify();
    return err;
}

void wifi_mgr_get_status(wifi_mgr_status_t *out)
{
    if (out == NULL) {
        return;
    }
    if (s_wifi.lock == NULL) {
        memset(out, 0, sizeof(*out));
        return;
    }
    if (s_wifi.status.state == WIFI_MGR_STATE_CONNECTED) {
        wifi_ap_record_t info;
        if (esp_wifi_sta_get_ap_info(&info) == ESP_OK) {
            xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
            s_wifi.status.rssi = info.rssi;
            xSemaphoreGive(s_wifi.lock);
        }
    }
    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    *out = s_wifi.status;
    xSemaphoreGive(s_wifi.lock);
}

int wifi_mgr_signal_level(void)
{
    wifi_mgr_status_t st;
    wifi_mgr_get_status(&st);
    if (st.state != WIFI_MGR_STATE_CONNECTED) {
        return 0;
    }
    if (st.rssi >= RSSI_LEVEL_3_DBM) {
        return 3;
    }
    return st.rssi >= RSSI_LEVEL_2_DBM ? 2 : 1;
}

esp_err_t wifi_mgr_scan(wifi_mgr_ap_record_t *out, size_t max, size_t *count)
{
    ESP_RETURN_ON_FALSE(out != NULL && count != NULL, ESP_ERR_INVALID_ARG, TAG, "bad args");
    ESP_RETURN_ON_FALSE(s_wifi.lock != NULL, ESP_ERR_INVALID_STATE, TAG, "not started");
    *count = 0;

    wifi_ap_record_t *records = calloc(SCAN_MAX_RECORDS, sizeof(*records));
    ESP_RETURN_ON_FALSE(records != NULL, ESP_ERR_NO_MEM, TAG, "no memory");

    s_wifi.scanning = true;
    esp_err_t err = esp_wifi_scan_start(NULL, true);
    uint16_t n = SCAN_MAX_RECORDS;
    if (err == ESP_OK) {
        err = esp_wifi_scan_get_ap_records(&n, records);
    }
    s_wifi.scanning = false;

    if (err == ESP_OK) {
        /* Records arrive sorted by RSSI; keep the strongest entry per SSID */
        for (uint16_t i = 0; i < n && *count < max; i++) {
            const char *ssid = (const char *)records[i].ssid;
            if (ssid[0] == '\0') {
                continue;
            }
            bool dup = false;
            for (size_t j = 0; j < *count && !dup; j++) {
                dup = strcmp(out[j].ssid, ssid) == 0;
            }
            if (dup) {
                continue;
            }
            strlcpy(out[*count].ssid, ssid, sizeof(out[*count].ssid));
            out[*count].rssi = records[i].rssi;
            out[*count].secure = records[i].authmode != WIFI_AUTH_OPEN;
            (*count)++;
        }
    }
    free(records);
    return err;
}
