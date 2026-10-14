// SPDX-License-Identifier: Apache-2.0
//
// Wi-Fi manager: station mode with stored credentials, falling back to a setup
// access point ("<prefix>-XXXX", captive DNS to the web config page) when no
// network is configured or the connection keeps failing.
//
// Singleton: it owns the ESP-IDF Wi-Fi driver and its netifs. All functions are
// thread-safe. The status callback runs in the default event loop task (or the
// manager's timer task); keep it short and never block in it.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MGR_SSID_LEN  33
#define WIFI_MGR_IP_LEN    16

typedef enum {
    WIFI_MGR_STATE_IDLE = 0,       /* no credentials, setup AP only */
    WIFI_MGR_STATE_CONNECTING,
    WIFI_MGR_STATE_CONNECTED,
    WIFI_MGR_STATE_FAILED,         /* gave up for now; setup AP is running */
} wifi_mgr_state_t;

typedef struct {
    wifi_mgr_state_t state;
    char ssid[WIFI_MGR_SSID_LEN];      /* station network (configured or connected) */
    char ip[WIFI_MGR_IP_LEN];          /* station IP, "" when not connected */
    int8_t rssi;                       /* station signal in dBm, 0 when not connected */
    bool ap_active;
    char ap_ssid[WIFI_MGR_SSID_LEN];
    char ap_ip[WIFI_MGR_IP_LEN];
} wifi_mgr_status_t;

typedef struct {
    char ssid[WIFI_MGR_SSID_LEN];
    int8_t rssi;
    bool secure;
} wifi_mgr_ap_record_t;

typedef void (*wifi_mgr_status_cb_t)(const wifi_mgr_status_t *status, void *user_ctx);

typedef struct {
    const char *ap_ssid_prefix;        /* setup AP name prefix, MAC suffix is appended */
    const char *ap_password;           /* "" or NULL for an open setup AP, else >= 8 chars */
    uint8_t max_retries;               /* station attempts before enabling the setup AP */
    wifi_mgr_status_cb_t status_cb;
    void *user_ctx;
} wifi_mgr_config_t;

/** Start Wi-Fi. Connects with @p ssid / @p password, or opens the setup AP if @p ssid is empty. */
esp_err_t wifi_mgr_start(const wifi_mgr_config_t *config, const char *ssid, const char *password);

/** Switch to new station credentials (the setup AP stays up until they work). */
esp_err_t wifi_mgr_connect(const char *ssid, const char *password);

/** Enable the setup AP now (e.g. from a button or the device settings). */
esp_err_t wifi_mgr_open_setup_ap(void);

/** Copy the current status. */
void wifi_mgr_get_status(wifi_mgr_status_t *out);

/** Blocking scan; fills up to @p max unique networks sorted by signal. */
esp_err_t wifi_mgr_scan(wifi_mgr_ap_record_t *out, size_t max, size_t *count);

/** Signal bars 0..3 for the current station RSSI (0 when disconnected). */
int wifi_mgr_signal_level(void);

#ifdef __cplusplus
}
#endif
