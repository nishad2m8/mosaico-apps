// SPDX-License-Identifier: Apache-2.0
//
// ISS tracker for ESP-Mosaico.
//
// Ownership: app_main starts Vibe Mode support, the GSP UI and iss_net, then
// hands the screen to one app task. That task is the single owner of the
// iss_ui handle: it applies new orbit data from iss_net and renders a frame
// every FRAME_PERIOD_MS.

#include <string.h>
#include <sys/time.h>

#include "board_display.h"
#include "bsp/esp_mosaico.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iris_ota_support.h"
#include "iris_screen_mirror.h"
#include "iss_net.h"
#include "iss_tle_default.h"
#include "iss_ui.h"
#include "nvs_flash.h"
#include "ui_bundle.h"

static const char *TAG = "iss_tracker";

#define FRAME_PERIOD_MS  50    /* up to 20 frames/s while the globe moves */
#define APP_TASK_STACK   8192
#define APP_TASK_PRIO    4
#define TIMING_LOG_FRAMES 600   /* log the average update time every ~30 s */

static int64_t wall_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static const char *link_text(iss_net_state_t state, bool time_valid)
{
    switch (state) {
    case ISS_NET_NO_WIFI:    return time_valid ? "OFFLINE" : "NO WI-FI · SET IT IN VIBE MODE";
    case ISS_NET_CONNECTING: return time_valid ? "RECONNECTING" : "CONNECTING";
    case ISS_NET_ONLINE:     return time_valid ? "LIVE" : "SYNCING TIME";
    }
    return "";
}

/* Prefer orbit data saved from an earlier download when it is newer than the built-in set. */
static void apply_saved_tle(iss_ui_handle_t ui)
{
    static char l1[ISS_NET_TLE_LINE_SIZE], l2[ISS_NET_TLE_LINE_SIZE];
    static sgp4_record_t saved, builtin;
    if (iss_net_load_saved_tle(l1, l2) == ESP_OK &&
            sgp4_init(&saved, l1, l2) == SGP4_OK &&
            sgp4_init(&builtin, ISS_TLE_DEFAULT_LINE1, ISS_TLE_DEFAULT_LINE2) == SGP4_OK &&
            saved.epoch_unix_ms > builtin.epoch_unix_ms) {
        iss_ui_set_tle(ui, l1, l2);
        ESP_LOGI(TAG, "using saved orbit data");
    }
}

static void app_task(void *arg)
{
    iss_ui_handle_t ui = arg;
    static char l1[ISS_NET_TLE_LINE_SIZE], l2[ISS_NET_TLE_LINE_SIZE];
    iss_net_state_t shown_state = (iss_net_state_t)-1;
    bool shown_time = false;
    apply_saved_tle(ui);
    TickType_t last = xTaskGetTickCount();
    int64_t busy_us = 0, worst_us = 0;
    unsigned frames = 0;
    for (;;) {
        if (iss_net_take_tle(l1, l2) && iss_ui_set_tle(ui, l1, l2) != ESP_GSP_OK) {
            ESP_LOGW(TAG, "downloaded orbit data rejected");
        }
        const bool time_valid = iss_net_time_valid();
        const iss_net_state_t state = iss_net_get_state();
        if (state != shown_state || time_valid != shown_time) {
            iss_ui_set_link(ui, link_text(state, time_valid));
            shown_state = state;
            shown_time = time_valid;
        }
        const int64_t t0 = esp_timer_get_time();
        const esp_gsp_err_t err = iss_ui_update(ui, wall_time_ms(), time_valid);
        const int64_t spent = esp_timer_get_time() - t0;
        if (err != ESP_GSP_OK) {
            ESP_LOGW(TAG, "frame update failed (%d)", (int)err);
        }
        busy_us += spent;
        worst_us = spent > worst_us ? spent : worst_us;
        if (++frames == TIMING_LOG_FRAMES) {
            ESP_LOGI(TAG, "frame update avg %lld ms, worst %lld ms",
                     (long long)(busy_us / frames / 1000), (long long)(worst_us / 1000));
            busy_us = worst_us = 0;
            frames = 0;
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(FRAME_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Keep Recovery reachable even when the external UI image is absent or
     * invalid. */
    iris_ota_support_start();

    esp_gsp_config_t app_config;
    const esp_err_t bundle_err = ui_bundle_open(&app_config);
    if (bundle_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "GSP UI unavailable (0x%x); ESP-Iris Recovery RPC remains active",
                 bundle_err);
        return;
    }

    esp_display_present_target_config_t display;
    ESP_ERROR_CHECK(board_display_init(&display));

    esp_lcd_touch_handle_t touch = NULL;
    ESP_ERROR_CHECK(board_touch_init(&touch));

    esp_gsp_esp_lcd_config_t lcd = ESP_GSP_ESP_LCD_CONFIG_INIT();
    lcd.display = display;
    lcd.touch = touch;

    ESP_ERROR_CHECK(iris_screen_mirror_init());

    esp_gsp_handle_t gsp;
    ESP_ERROR_CHECK(esp_gsp_esp_lcd_start(&app_config, &lcd, &gsp));
    ESP_ERROR_CHECK(iris_screen_mirror_attach(gsp));

    iss_ui_handle_t ui = NULL;
    const esp_gsp_err_t ui_err = iss_ui_create(gsp, &ui);
    if (ui_err != ESP_GSP_OK) {
        ESP_LOGE(TAG, "ISS screen unavailable (%d)", (int)ui_err);
        return;
    }

    /* The tracker still runs from built-in data when Wi-Fi is unavailable. */
    ESP_ERROR_CHECK_WITHOUT_ABORT(iss_net_start());

    if (xTaskCreate(app_task, "iss_app", APP_TASK_STACK, ui, APP_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "app task not created");
        iss_ui_delete(ui);
        return;
    }
    ESP_LOGI(TAG, "ISS tracker ready at %dx%d RGB565", BSP_LCD_H_RES, BSP_LCD_V_RES);
}
