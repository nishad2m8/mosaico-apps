// SPDX-License-Identifier: Apache-2.0

#include "board_power.h"

#include <stdlib.h>
#include "bsp/esp_mosaico.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "board_power";

#define SHUTDOWN_ASSERT_MS   1500   /* long enough for the SAM8108 to cut power */
#define CHARGE_CURRENT_MIN_MA 5     /* ignore gauge noise around 0 mA */

static struct {
    SemaphoreHandle_t lock;
    bool gauge_ok;
} s_power;

esp_err_t board_power_init(void)
{
    if (s_power.lock != NULL) {
        return ESP_OK;
    }
    s_power.lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_power.lock != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    ESP_RETURN_ON_ERROR(bsp_power_init(), TAG, "power rails");

    esp_err_t err = bsp_battery_init();
    s_power.gauge_ok = err == ESP_OK;
    if (!s_power.gauge_ok) {
        ESP_LOGW(TAG, "fuel gauge unavailable: %s", esp_err_to_name(err));
    }
    return ESP_OK;
}

esp_err_t board_power_read_battery(board_battery_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "out is null");
    ESP_RETURN_ON_FALSE(s_power.lock != NULL, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    if (!s_power.gauge_ok) {
        return ESP_ERR_NOT_FOUND;
    }

    /* bsp_battery_status_t is ~60 bytes; fine on the caller's stack */
    bsp_battery_status_t st;
    xSemaphoreTake(s_power.lock, portMAX_DELAY);
    esp_err_t err = bsp_battery_read(&st);
    xSemaphoreGive(s_power.lock);
    ESP_RETURN_ON_ERROR(err, TAG, "gauge read");

    int soc = st.state_of_charge;
    out->percent = soc < 0 ? 0 : (soc > 100 ? 100 : soc);
    out->voltage_mv = st.voltage_mv;
    out->current_ma = st.current_ma;
    out->charging = st.current_ma > CHARGE_CURRENT_MIN_MA;
    return ESP_OK;
}

esp_err_t board_power_off(void (*before_off)(void))
{
    ESP_LOGW(TAG, "power-off requested");
    if (before_off != NULL) {
        before_off();
    }
    ESP_RETURN_ON_ERROR(bsp_power_set_shutdown(true), TAG, "shutdown request");
    vTaskDelay(pdMS_TO_TICKS(SHUTDOWN_ASSERT_MS));

    /* Still running: external power keeps the board alive. Release PWR_SW so
     * the POWER key and the next request behave normally. */
    bsp_power_set_shutdown(false);
    ESP_LOGW(TAG, "board still powered (USB?); power-off not possible now");
    return ESP_ERR_INVALID_STATE;
}
