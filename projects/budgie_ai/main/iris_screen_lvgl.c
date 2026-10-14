// SPDX-License-Identifier: Apache-2.0
//
// ESP-Iris calls begin/read/end from its worker task, one capture at a time.
// That task has a small stack, so it must never render: begin() queues the
// snapshot onto the LVGL task (lv_async_call) and waits for it. The frame
// buffer is allocated once at registration so a late LVGL callback (after a
// timeout) can never write into freed memory.

#include "iris_screen_lvgl.h"

#include <string.h>
#include "bsp/esp_mosaico.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"

static const char *TAG = "iris_screen";

#define BYTES_PER_PIXEL   2
#define FRAME_STRIDE      (BSP_LCD_H_RES * BYTES_PER_PIXEL)
#define FRAME_BYTES       (FRAME_STRIDE * BSP_LCD_V_RES)
#define LOCK_TIMEOUT_MS   2000
#define CAPTURE_TIMEOUT_MS 3000

static struct {
    uint8_t *frame;               /* FRAME_BYTES, PSRAM, lives for the whole run */
    SemaphoreHandle_t done;       /* given by the LVGL task when a snapshot finishes */
    volatile bool pending;        /* a snapshot is queued or running on the LVGL task */
    volatile esp_err_t result;
} s_cap;

/* Runs on the LVGL task (inside lv_timer_handler) */
static void snapshot_on_lvgl_task(void *arg)
{
    esp_err_t result = ESP_ERR_NO_MEM;
    lv_draw_buf_t *snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    if (snap != NULL) {
        const uint32_t w = snap->header.w < BSP_LCD_H_RES ? snap->header.w : BSP_LCD_H_RES;
        const uint32_t h = snap->header.h < BSP_LCD_V_RES ? snap->header.h : BSP_LCD_V_RES;
        memset(s_cap.frame, 0, FRAME_BYTES);
        for (uint32_t y = 0; y < h; y++) {
            memcpy(s_cap.frame + y * FRAME_STRIDE, snap->data + y * snap->header.stride,
                   w * BYTES_PER_PIXEL);
        }
        lv_draw_buf_destroy(snap);
        result = ESP_OK;
    }
    s_cap.result = result;
    s_cap.pending = false;
    xSemaphoreGive(s_cap.done);
}

static esp_err_t screen_begin(const esp_iris_media_desc_t *requested, esp_iris_media_desc_t *actual,
                              uint32_t *total_size, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(actual != NULL && total_size != NULL, ESP_ERR_INVALID_ARG, TAG, "bad args");
    ESP_RETURN_ON_FALSE(!s_cap.pending, ESP_ERR_INVALID_STATE, TAG, "previous capture still running");

    xSemaphoreTake(s_cap.done, 0);   /* drop a completion left by a timed-out capture */
    ESP_RETURN_ON_FALSE(bsp_display_lock(LOCK_TIMEOUT_MS), ESP_ERR_TIMEOUT, TAG, "display lock");
    s_cap.pending = true;
    const lv_result_t queued = lv_async_call(snapshot_on_lvgl_task, NULL);
    bsp_display_unlock();
    if (queued != LV_RESULT_OK) {
        s_cap.pending = false;
        return ESP_ERR_NO_MEM;
    }
    ESP_RETURN_ON_FALSE(xSemaphoreTake(s_cap.done, pdMS_TO_TICKS(CAPTURE_TIMEOUT_MS)) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "snapshot timed out");
    ESP_RETURN_ON_ERROR(s_cap.result, TAG, "snapshot failed");

    *actual = (esp_iris_media_desc_t) {
        .width = BSP_LCD_H_RES,
        .height = BSP_LCD_V_RES,
        .stride = FRAME_STRIDE,
        .format = ESP_IRIS_PIXEL_FORMAT_RGB565,
    };
    *total_size = FRAME_BYTES;
    return ESP_OK;
}

static esp_err_t screen_read(uint32_t offset, uint8_t *out, size_t capacity, size_t *out_size,
                             void *user_ctx)
{
    ESP_RETURN_ON_FALSE(out != NULL && out_size != NULL && offset < FRAME_BYTES && !s_cap.pending,
                        ESP_ERR_INVALID_ARG, TAG, "bad read");
    size_t n = FRAME_BYTES - offset;
    if (n > capacity) {
        n = capacity;
    }
    memcpy(out, s_cap.frame + offset, n);
    *out_size = n;
    return ESP_OK;
}

static void screen_end(void *user_ctx)
{
    /* The frame buffer is kept for the next capture */
}

esp_err_t iris_screen_lvgl_register(void)
{
    ESP_RETURN_ON_FALSE(s_cap.frame == NULL, ESP_ERR_INVALID_STATE, TAG, "already registered");
    s_cap.frame = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_cap.done = xSemaphoreCreateBinary();
    if (s_cap.frame == NULL || s_cap.done == NULL) {
        heap_caps_free(s_cap.frame);
        if (s_cap.done != NULL) {
            vSemaphoreDelete(s_cap.done);
        }
        s_cap.frame = NULL;
        s_cap.done = NULL;
        return ESP_ERR_NO_MEM;
    }
    const esp_iris_screen_backend_t backend = {
        .begin = screen_begin,
        .read = screen_read,
        .end = screen_end,
        .user_ctx = &s_cap,
    };
    return esp_iris_screen_register(&backend);
}
