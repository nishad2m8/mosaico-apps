// SPDX-License-Identifier: Apache-2.0
//
// Web configuration server (port 80):
//   GET  /                 single-page settings UI (embedded)
//   GET  /api/status       device, Wi-Fi and battery status
//   GET  /api/config       settings (API keys are never returned, only "set" flags)
//   POST /api/config       partial settings update
//   GET  /api/wifi/scan    nearby networks
//   POST /api/wifi         {"ssid","password"} save and connect
//   POST /api/test         list models with the given/current AI settings
//   POST /api/chat         {"text"} ask Budgie (answer appears on the device)
//   POST /api/action       {"action":"reboot"|"vibe_mode"|"clear_history"|"factory_reset"}
// Unknown paths redirect to "/" so phones on the setup AP open the page.
//
// Singleton. Handlers run in the httpd task; callbacks are invoked from it.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WEB_ACTION_REBOOT = 0,
    WEB_ACTION_VIBE_MODE,
    WEB_ACTION_CLEAR_HISTORY,
    WEB_ACTION_FACTORY_RESET,
} web_config_action_t;

typedef struct {
    int battery_percent;         /* -1 when unknown */
    bool charging;
    const char *app_version;
} web_config_device_info_t;

typedef struct {
    /* Fill live device info for /api/status */
    void (*get_device_info)(web_config_device_info_t *out, void *user_ctx);
    /* Ask the assistant; return ESP_ERR_INVALID_STATE when busy */
    esp_err_t (*send_chat)(const char *text, void *user_ctx);
    /* Run a maintenance action; may restart the device after returning */
    void (*run_action)(web_config_action_t action, void *user_ctx);
    void *user_ctx;
} web_config_config_t;

esp_err_t web_config_start(const web_config_config_t *config);
void web_config_stop(void);

#ifdef __cplusplus
}
#endif
