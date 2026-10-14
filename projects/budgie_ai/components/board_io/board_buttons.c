// SPDX-License-Identifier: Apache-2.0

#include "board_buttons.h"

#include <stdint.h>
#include "bsp/esp_mosaico.h"
#include "button_gpio.h"
#include "esp_check.h"
#include "iot_button.h"

static const char *TAG = "board_buttons";

#define BOOT_LONG_PRESS_MS   1500
#define BOOT_SHORT_PRESS_MS  180

/* Owned by board_buttons_start(); read-only afterwards */
static struct {
    board_buttons_cb_t cb;
    void *user_ctx;
    button_handle_t fn;
    button_handle_t boot;
} s_btn;

static void emit(void *button_handle, void *user_data)
{
    if (s_btn.cb != NULL) {
        s_btn.cb((board_button_event_t)(intptr_t)user_data, s_btn.user_ctx);
    }
}

static esp_err_t create_boot_button(button_handle_t *out)
{
    const button_config_t cfg = {
        .long_press_time = BOOT_LONG_PRESS_MS,
        .short_press_time = BOOT_SHORT_PRESS_MS,
    };
    const button_gpio_config_t gpio = {
        .gpio_num = BSP_BUTTON_BOOT_GPIO,
        .active_level = BSP_BUTTON_ACTIVE_LEVEL,
    };
    return iot_button_new_gpio_device(&cfg, &gpio, out);
}

esp_err_t board_buttons_start(board_buttons_cb_t cb, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(s_btn.fn == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");
    s_btn.cb = cb;
    s_btn.user_ctx = user_ctx;

    button_handle_t buttons[BSP_BUTTON_NUM] = { 0 };
    int count = 0;
    ESP_RETURN_ON_ERROR(bsp_iot_button_create(buttons, &count, BSP_BUTTON_NUM), TAG, "AI button");
    ESP_RETURN_ON_FALSE(count > BSP_BUTTON_AI && buttons[BSP_BUTTON_AI] != NULL,
                        ESP_ERR_NOT_FOUND, TAG, "AI button missing");
    s_btn.fn = buttons[BSP_BUTTON_AI];

    ESP_RETURN_ON_ERROR(iot_button_register_cb(s_btn.fn, BUTTON_PRESS_DOWN, NULL, emit,
                        (void *)(intptr_t)BOARD_BTN_FN_DOWN), TAG, "fn down");
    ESP_RETURN_ON_ERROR(iot_button_register_cb(s_btn.fn, BUTTON_PRESS_UP, NULL, emit,
                        (void *)(intptr_t)BOARD_BTN_FN_UP), TAG, "fn up");

    esp_err_t err = create_boot_button(&s_btn.boot);
    if (err != ESP_OK) {
        /* Boot is optional for the app; push-to-talk still works without it */
        ESP_LOGW(TAG, "Boot button unavailable: %s", esp_err_to_name(err));
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(iot_button_register_cb(s_btn.boot, BUTTON_SINGLE_CLICK, NULL, emit,
                        (void *)(intptr_t)BOARD_BTN_BOOT_CLICK), TAG, "boot click");
    ESP_RETURN_ON_ERROR(iot_button_register_cb(s_btn.boot, BUTTON_LONG_PRESS_START, NULL, emit,
                        (void *)(intptr_t)BOARD_BTN_BOOT_LONG), TAG, "boot long");
    return ESP_OK;
}
