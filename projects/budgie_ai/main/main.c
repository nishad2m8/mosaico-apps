// SPDX-License-Identifier: Apache-2.0
//
// Budgie: OpenAI-compatible voice/text chat assistant for ESP-Mosaico.
//
// Ownership: app_main creates every service once and keeps the handles in
// `s_app`; they live for the whole run. Module callbacks below are thin
// adapters that forward events between services:
//   assistant  -> app_ui (mode, transcript, reply)
//   wifi_mgr   -> app_ui (status bar, setup hints), SNTP
//   buttons    -> assistant (push-to-talk) / app_ui (wake, home)
//   web_config -> app_config / assistant / maintenance actions
//   app_config -> audio volume, time zone, UI controls

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "app_config.h"
#include "app_ui.h"
#include "assistant.h"
#include "audio_io.h"
#include "board_buttons.h"
#include "board_power.h"
#include "bsp/esp_mosaico.h"
#include "camera_io.h"
#include "esp_app_desc.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iris_ota_support.h"
#include "iris_screen_lvgl.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "web_config.h"
#include "wifi_manager.h"

static const char *TAG = "budgie";

#define SETUP_AP_PREFIX          "Budgie"
#define SETUP_AP_PASSWORD        ""          /* open setup network; set >= 8 chars for WPA2 */
#define WIFI_MAX_RETRIES         5
#define PUSH_TO_TALK_MIN_MS      600         /* shorter Fn presses toggle listening instead */
#define MONITOR_PERIOD_MS        10000
#define BATTERY_EVERY_N_PERIODS  3
#define MONITOR_STACK            3072
#define POWER_TASK_STACK         3072
#define RESTART_DELAY_US         (800 * 1000)
#define SNTP_SERVER              "pool.ntp.org"
#define CAMERA_JPEG_QUALITY      70          /* ~30-60 KB photos for the vision model */

/* Vibe Mode stores the Wi-Fi it was given here; reused when the app has none */
#define VIBE_NVS_PARTITION       "sysmeta"
#define VIBE_NVS_NAMESPACE       "wifi"

static struct {
    audio_io_handle_t audio;
    camera_io_handle_t camera;
    assistant_handle_t assistant;
    esp_timer_handle_t restart_timer;
    bool sntp_started;
    bool fn_started_listening;    /* Fn press began this recording */
    bool fn_ignore_release;       /* Fn press ended a toggled recording */
    int64_t fn_down_us;
    volatile bool powering_off;
} s_app;

/* ---------- Assistant / UI adapters ---------- */

static void on_assistant_event(const assistant_event_t *event, void *ctx)
{
    app_ui_post_assistant_event(event);
}

/* Mic: tap to listen, tap Stop to hold the recording, tap Send to ask */
static void ui_on_mic(void *ctx)
{
    esp_err_t err;
    if (assistant_is_listening(s_app.assistant)) {
        err = assistant_listen_hold(s_app.assistant);
    } else if (assistant_has_recording(s_app.assistant)) {
        err = assistant_send_recording(s_app.assistant);
    } else {
        err = assistant_listen_start(s_app.assistant);
    }
    if (err == ESP_ERR_INVALID_STATE) {
        app_ui_post_notice("One moment, I'm still thinking...");
    }
}

static void ui_on_clear_recording(void *ctx)
{
    assistant_listen_cancel(s_app.assistant);
}

static esp_err_t ui_on_ask(const assistant_question_t *question, void *ctx)
{
    return assistant_ask(s_app.assistant, question);
}

static void ui_on_clear_history(void *ctx)
{
    assistant_clear_history(s_app.assistant);
}

static size_t ui_list_history(chat_history_entry_t *out, size_t out_max, void *ctx)
{
    return assistant_history_list(s_app.assistant, out, out_max);
}

static void ui_on_delete_history(uint32_t id, void *ctx)
{
    assistant_history_remove(s_app.assistant, id);
}

/* ---------- Power off ---------- */

static void before_power_off(void)
{
    assistant_listen_cancel(s_app.assistant);
    assistant_stop_speaking(s_app.assistant);
    audio_io_play_end(s_app.audio);
    app_ui_blank_display();
}

static void power_off_task(void *arg)
{
    if (board_power_off(before_power_off) != ESP_OK) {
        /* Still running: USB keeps the board powered */
        app_ui_post_config_changed();   /* restores brightness */
        app_ui_post_notice("Still powered by USB. Unplug it and try again, or use the power key.");
    }
    s_app.powering_off = false;
    vTaskDelete(NULL);
}

static void ui_on_power_off(void *ctx)
{
    if (s_app.powering_off) {
        return;
    }
    s_app.powering_off = true;
    if (xTaskCreate(power_off_task, "power_off", POWER_TASK_STACK, NULL, 5, NULL) != pdPASS) {
        s_app.powering_off = false;
    }
}

/* ---------- Buttons ---------- */

/* Function button: hold to talk, or tap to start and tap again to send */
static void on_button(board_button_event_t event, void *ctx)
{
    app_ui_post_wake();
    switch (event) {
    case BOARD_BTN_FN_DOWN:
        s_app.fn_down_us = esp_timer_get_time();
        if (assistant_is_listening(s_app.assistant)) {
            s_app.fn_ignore_release = true;
            s_app.fn_started_listening = false;
            assistant_listen_stop(s_app.assistant);
        } else {
            s_app.fn_ignore_release = false;
            s_app.fn_started_listening = assistant_listen_start(s_app.assistant) == ESP_OK;
        }
        break;
    case BOARD_BTN_FN_UP: {
        const int64_t held_ms = (esp_timer_get_time() - s_app.fn_down_us) / 1000;
        if (!s_app.fn_ignore_release && s_app.fn_started_listening && held_ms >= PUSH_TO_TALK_MIN_MS) {
            assistant_listen_stop(s_app.assistant);
        }
        s_app.fn_started_listening = false;
        s_app.fn_ignore_release = false;
        break;
    }
    case BOARD_BTN_BOOT_CLICK:
    case BOARD_BTN_BOOT_LONG:
        /* Harmless on purpose: Boot is also the ROM Download Mode key */
        assistant_listen_cancel(s_app.assistant);
        assistant_stop_speaking(s_app.assistant);
        app_ui_post_go_home();
        break;
    }
}

/* ---------- Wi-Fi / time ---------- */

static void apply_timezone(const char *tz)
{
    setenv("TZ", (tz != NULL && tz[0] != '\0') ? tz : "UTC0", 1);
    tzset();
}

static void on_wifi_status(const wifi_mgr_status_t *status, void *ctx)
{
    app_ui_post_wifi(status);
    if (status->state == WIFI_MGR_STATE_CONNECTED && !s_app.sntp_started) {
        esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
        s_app.sntp_started = esp_netif_sntp_init(&sntp) == ESP_OK;
    }
}

/* Fall back to the network configured for Vibe Mode (mosaico.py recovery-wifi) */
static void load_vibe_wifi(app_config_t *cfg)
{
    nvs_handle_t nvs;
    if (nvs_open_from_partition(VIBE_NVS_PARTITION, VIBE_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    size_t ssid_len = sizeof(cfg->wifi_ssid);
    size_t pass_len = sizeof(cfg->wifi_pass);
    if (nvs_get_str(nvs, "ssid", cfg->wifi_ssid, &ssid_len) == ESP_OK) {
        if (nvs_get_str(nvs, "password", cfg->wifi_pass, &pass_len) != ESP_OK) {
            cfg->wifi_pass[0] = '\0';
        }
        ESP_LOGI(TAG, "using Vibe Mode Wi-Fi \"%s\"", cfg->wifi_ssid);
    } else {
        cfg->wifi_ssid[0] = '\0';
    }
    nvs_close(nvs);
}

/* ---------- Settings changes ---------- */

static void on_config_changed(void *ctx)
{
    app_config_t *cfg = app_config_alloc();
    if (cfg == NULL) {
        return;
    }
    app_config_get(cfg);
    audio_io_set_volume(s_app.audio, cfg->volume);
    apply_timezone(cfg->timezone);
    free(cfg);
    app_ui_post_config_changed();
}

/* ---------- Web page adapters ---------- */

static void web_device_info(web_config_device_info_t *out, void *ctx)
{
    board_battery_t batt;
    if (board_power_read_battery(&batt) == ESP_OK) {
        out->battery_percent = batt.percent;
        out->charging = batt.charging;
    }
    out->app_version = esp_app_get_description()->version;
}

static esp_err_t web_send_chat(const char *text, void *ctx)
{
    app_ui_post_go_home();
    return assistant_send_text(s_app.assistant, text);
}

static void restart_cb(void *arg)
{
    esp_restart();
}

static void web_run_action(web_config_action_t action, void *ctx)
{
    switch (action) {
    case WEB_ACTION_CLEAR_HISTORY:
        assistant_clear_history(s_app.assistant);
        app_ui_post_notice("Chat memory cleared");
        break;
    case WEB_ACTION_VIBE_MODE:
        app_ui_post_notice("Restarting into Vibe Mode...");
        if (iris_ota_support_enter_recovery() != ESP_OK) {
            app_ui_post_notice("Could not enter Vibe Mode");
        }
        break;
    case WEB_ACTION_FACTORY_RESET:
        app_config_reset();
        __attribute__((fallthrough));   /* restart with the defaults */
    case WEB_ACTION_REBOOT:
        app_ui_post_notice("Restarting...");
        esp_timer_start_once(s_app.restart_timer, RESTART_DELAY_US);
        break;
    }
}

/* ---------- Background monitor ---------- */

static void monitor_task(void *arg)
{
    for (unsigned tick = 0;; tick++) {
        if (tick % BATTERY_EVERY_N_PERIODS == 0) {
            board_battery_t batt;
            if (board_power_read_battery(&batt) == ESP_OK) {
                app_ui_post_battery(batt.percent);
            }
        }
        wifi_mgr_status_t st;
        wifi_mgr_get_status(&st);   /* refreshes RSSI for the signal bars */
        if (st.state == WIFI_MGR_STATE_CONNECTED) {
            app_ui_post_wifi(&st);
        }
        vTaskDelay(pdMS_TO_TICKS(MONITOR_PERIOD_MS));
    }
}

/* ---------- Startup ---------- */

static esp_err_t init_nvs(void)
{
    /* The application's own "nvs" partition only; Vibe Mode data lives in sysmeta */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "app NVS needs formatting (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

/* The camera module is optional: without it the camera screen explains how to attach one */
static void start_camera(void)
{
    const camera_io_config_t cam_cfg = {
        .preview_size = BSP_LCD_H_RES,
        .jpeg_quality = CAMERA_JPEG_QUALITY,
        .event_cb = app_ui_camera_event_cb,
    };
    esp_err_t err = camera_io_create(&cam_cfg, &s_app.camera);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "camera unavailable: %s", esp_err_to_name(err));
        return;
    }
    app_ui_attach_camera(s_app.camera);
}

static void start_network(const app_config_t *cfg)
{
    const wifi_mgr_config_t wifi_cfg = {
        .ap_ssid_prefix = SETUP_AP_PREFIX,
        .ap_password = SETUP_AP_PASSWORD,
        .max_retries = WIFI_MAX_RETRIES,
        .status_cb = on_wifi_status,
    };
    esp_err_t err = wifi_mgr_start(&wifi_cfg, cfg->wifi_ssid, cfg->wifi_pass);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi start failed: %s", esp_err_to_name(err));
        app_ui_post_notice("Wi-Fi failed to start");
        return;
    }
    const web_config_config_t web_cfg = {
        .get_device_info = web_device_info,
        .send_chat = web_send_chat,
        .run_action = web_run_action,
    };
    err = web_config_start(&web_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "web server failed: %s", esp_err_to_name(err));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(init_nvs());

    /* First, so Vibe Mode stays reachable over ESP-Iris even if anything below fails */
    iris_ota_support_start();

    ESP_ERROR_CHECK(app_config_init());
    app_config_t *cfg = app_config_alloc();
    ESP_ERROR_CHECK(cfg != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    app_config_get(cfg);
    apply_timezone(cfg->timezone);

    ESP_ERROR_CHECK_WITHOUT_ABORT(board_power_init());

    const app_ui_callbacks_t ui_cb = {
        .on_mic = ui_on_mic,
        .on_clear_recording = ui_on_clear_recording,
        .on_ask = ui_on_ask,
        .on_clear_history = ui_on_clear_history,
        .list_history = ui_list_history,
        .on_delete_history = ui_on_delete_history,
        .on_power_off = ui_on_power_off,
    };
    ESP_ERROR_CHECK(app_ui_start(&ui_cb, esp_app_get_description()->version));
    ESP_ERROR_CHECK_WITHOUT_ABORT(iris_screen_lvgl_register());   /* mosaico.py iris screenshot */
    start_camera();

    audio_io_config_t audio_cfg = AUDIO_IO_DEFAULT_CONFIG();
    audio_cfg.volume = cfg->volume;
    ESP_ERROR_CHECK(audio_io_create(&audio_cfg, &s_app.audio));

    const assistant_config_t assistant_cfg = {
        .audio = s_app.audio,
        .event_cb = on_assistant_event,
    };
    ESP_ERROR_CHECK(assistant_create(&assistant_cfg, &s_app.assistant));
    app_config_register_cb(on_config_changed, NULL);

    const esp_timer_create_args_t restart_args = { .callback = restart_cb, .name = "restart" };
    ESP_ERROR_CHECK(esp_timer_create(&restart_args, &s_app.restart_timer));

    if (cfg->wifi_ssid[0] == '\0') {
        load_vibe_wifi(cfg);
    }
    start_network(cfg);
    free(cfg);

    esp_err_t err = board_buttons_start(on_button, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "buttons unavailable: %s (touch UI still works)", esp_err_to_name(err));
    }
    if (xTaskCreate(monitor_task, "monitor", MONITOR_STACK, NULL, 2, NULL) != pdPASS) {
        ESP_LOGW(TAG, "monitor task not started");
    }
    ESP_LOGI(TAG, "Budgie ready");
}
