// SPDX-License-Identifier: Apache-2.0
//
// Pipeline (same as the BSP camera_photo_app example): UYVY sensor frame ->
// PPA center crop + scale + 90 degree rotation -> RGB565 preview buffer
// (the BSP example uses 270; in this case 270 shows the picture upside down).
// A photo is the preview frame encoded as JPEG with esp_new_jpeg.
//
// Everything below is owned by the camera task, except `cmds`.

#include "camera_io.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_enc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "linux/videodev2.h"
#include "mosaico_module_camera.h"

static const char *TAG = "camera_io";

#define TASK_STACK          6144
#define TASK_PRIO           4
#define CMD_QUEUE_LEN       8
#define BUFFER_ALIGNMENT    128          /* PPA / cache line alignment */
#define JPEG_MAX_BYTES      (160 * 1024)
#define FRAME_ERROR_LIMIT   10           /* consecutive failures before giving up */
#define PPA_SCALE_STEPS     16           /* PPA scale factors have a 1/16 fraction */

typedef enum {
    CMD_START,
    CMD_STOP,
    CMD_CAPTURE,
    CMD_RESUME,
} cmd_t;

struct camera_io_t {
    camera_io_config_t config;
    QueueHandle_t cmds;
    TaskHandle_t task;
    ppa_client_handle_t ppa;
    mosaico_camera_handle_t camera;
    uint16_t *preview[2];
    size_t preview_bytes;
    int back;                    /* index of the buffer written next */
    uint8_t *jpeg;
    bool streaming;
    bool frozen;
    bool capture_requested;
    int frame_errors;
    bool first_frame_logged;     /* once per stream, so a stall is visible in the log */
};

static void emit(camera_io_handle_t h, const camera_io_event_t *evt)
{
    if (h->config.event_cb != NULL) {
        h->config.event_cb(evt, h->config.user_ctx);
    }
}

static void emit_type(camera_io_handle_t h, camera_io_event_type_t type)
{
    const camera_io_event_t evt = { .type = type, .size = h->config.preview_size };
    emit(h, &evt);
}

static uint32_t align_down_even(uint32_t v)
{
    return v & ~1U;
}

/* Largest even square crop (<= max_crop) that the PPA scales to exactly `size`.
 * The PPA rounds the scale down to 1/16 steps, so a plain size/crop factor
 * (480/720 -> 10/16) leaves the preview short with black right/bottom edges.
 * Returns 0 when no such crop exists (e.g. a sensor smaller than the preview). */
static uint32_t exact_crop(uint32_t max_crop, uint32_t size)
{
    const uint32_t target = size * PPA_SCALE_STEPS;
    for (uint32_t steps = (target + max_crop - 1) / max_crop; steps <= PPA_SCALE_STEPS; steps++) {
        const uint32_t crop = target / steps;
        if (target % steps == 0 && (crop % 2U) == 0 && crop <= max_crop) {
            return crop;
        }
    }
    return 0;
}

/* ---------- Camera open / close ---------- */

static void close_camera(camera_io_handle_t h)
{
    if (h->camera == NULL) {
        return;
    }
    if (h->streaming) {
        mosaico_camera_stop_stream(h->camera);
    }
    mosaico_camera_close(h->camera);
    mosaico_camera_del(h->camera);
    h->camera = NULL;
    h->streaming = false;
    h->frozen = false;
    h->capture_requested = false;
}

static void open_camera(camera_io_handle_t h)
{
    if (h->streaming) {
        emit_type(h, CAMERA_IO_EVT_STARTED);
        return;
    }
    mosaico_camera_config_t cfg = MOSAICO_CAMERA_DEFAULT_CONFIG();
    esp_err_t err = mosaico_camera_new(&cfg, &h->camera);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no camera module: %s", esp_err_to_name(err));
        h->camera = NULL;
        emit_type(h, CAMERA_IO_EVT_NOT_FOUND);
        return;
    }
    err = mosaico_camera_open(h->camera);
    if (err == ESP_OK) {
        err = mosaico_camera_start_stream(h->camera);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "camera start failed: %s", esp_err_to_name(err));
        close_camera(h);
        emit_type(h, CAMERA_IO_EVT_ERROR);
        return;
    }
    h->streaming = true;
    h->frozen = false;
    h->frame_errors = 0;
    h->first_frame_logged = false;
    ESP_LOGI(TAG, "camera streaming");
    emit_type(h, CAMERA_IO_EVT_STARTED);
}

/* ---------- Frame processing ---------- */

static esp_err_t convert_frame(camera_io_handle_t h, const mosaico_camera_frame_t *frame, uint16_t *out)
{
    ESP_RETURN_ON_FALSE(frame->pixel_format == V4L2_PIX_FMT_UYVY, ESP_ERR_NOT_SUPPORTED, TAG,
                        "unexpected pixel format 0x%08x", (unsigned)frame->pixel_format);
    const uint16_t size = h->config.preview_size;
    const uint32_t max_crop = align_down_even(frame->width < frame->height ? frame->width : frame->height);
    const uint32_t exact = exact_crop(max_crop, size);
    const uint32_t crop = exact != 0 ? exact : max_crop;
    const uint32_t stride = frame->bytes_per_line ? frame->bytes_per_line : frame->width * 2U;
    ESP_RETURN_ON_FALSE(crop > 0 && (stride % 2U) == 0, ESP_ERR_INVALID_SIZE, TAG, "bad frame geometry");

    ESP_RETURN_ON_ERROR(esp_cache_msync((void *)frame->data, frame->size,
                        ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE), TAG, "cache in");
    ESP_RETURN_ON_ERROR(esp_cache_msync(out, h->preview_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M), TAG, "cache out");

    const ppa_srm_oper_config_t op = {
        .in = {
            .buffer = frame->data,
            .pic_w = stride / 2U,
            .pic_h = frame->height,
            .block_w = crop,
            .block_h = crop,
            .block_offset_x = align_down_even((frame->width - crop) / 2U),
            .block_offset_y = align_down_even((frame->height - crop) / 2U),
            .srm_cm = PPA_SRM_COLOR_MODE_YUV422_UYVY,
            .yuv_range = PPA_COLOR_RANGE_LIMIT,
            .yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601,
        },
        .out = {
            .buffer = out,
            .buffer_size = h->preview_bytes,
            .pic_w = size,
            .pic_h = size,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_90,
        .scale_x = (float)size / (float)crop,
        .scale_y = (float)size / (float)crop,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    ESP_RETURN_ON_ERROR(ppa_do_scale_rotate_mirror(h->ppa, &op), TAG, "PPA convert");
    return esp_cache_msync(out, h->preview_bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
}

static esp_err_t encode_jpeg(camera_io_handle_t h, const uint16_t *rgb565, int *out_len)
{
    jpeg_enc_config_t cfg = DEFAULT_JPEG_ENC_CONFIG();
    cfg.width = h->config.preview_size;
    cfg.height = h->config.preview_size;
    cfg.src_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    cfg.subsampling = JPEG_SUBSAMPLE_420;
    cfg.quality = h->config.jpeg_quality;

    jpeg_enc_handle_t enc = NULL;
    ESP_RETURN_ON_FALSE(jpeg_enc_open(&cfg, &enc) == JPEG_ERR_OK, ESP_FAIL, TAG, "JPEG open");
    const jpeg_error_t ret = jpeg_enc_process(enc, (const uint8_t *)rgb565, (int)h->preview_bytes,
                                              h->jpeg, JPEG_MAX_BYTES, out_len);
    jpeg_enc_close(enc);
    ESP_RETURN_ON_FALSE(ret == JPEG_ERR_OK && *out_len > 0, ESP_FAIL, TAG, "JPEG encode (%d)", ret);
    return ESP_OK;
}

static void process_frame(camera_io_handle_t h)
{
    mosaico_camera_frame_t frame;
    const esp_err_t got = mosaico_camera_get_frame(h->camera, &frame);
    if (got != ESP_OK) {
        ESP_LOGW(TAG, "no frame (%d/%d): %s", h->frame_errors + 1, FRAME_ERROR_LIMIT, esp_err_to_name(got));
        if (++h->frame_errors >= FRAME_ERROR_LIMIT) {
            ESP_LOGE(TAG, "camera stopped delivering frames");
            close_camera(h);
            emit_type(h, CAMERA_IO_EVT_ERROR);
        }
        return;
    }
    h->frame_errors = 0;
    if (!h->first_frame_logged) {
        h->first_frame_logged = true;
        ESP_LOGI(TAG, "first frame %ux%u stride %u fmt 0x%08x size %u", (unsigned)frame.width,
                 (unsigned)frame.height, (unsigned)frame.bytes_per_line, (unsigned)frame.pixel_format,
                 (unsigned)frame.size);
    }
    if (h->frozen) {
        mosaico_camera_return_frame(h->camera, &frame);   /* keep the pipeline drained */
        return;
    }
    uint16_t *out = h->preview[h->back];
    const esp_err_t err = convert_frame(h, &frame, out);
    mosaico_camera_return_frame(h->camera, &frame);
    if (err != ESP_OK) {
        return;
    }

    camera_io_event_t evt = { .type = CAMERA_IO_EVT_FRAME, .rgb565 = out, .size = h->config.preview_size };
    emit(h, &evt);

    int len = 0;
    if (h->capture_requested) {
        h->capture_requested = false;
        if (encode_jpeg(h, out, &len) == ESP_OK) {
            h->frozen = true;   /* keep showing exactly what was sent; `out` is not reused */
            evt.type = CAMERA_IO_EVT_PHOTO;
            evt.jpeg = h->jpeg;
            evt.jpeg_len = (size_t)len;
            ESP_LOGI(TAG, "photo %d bytes", len);
            emit(h, &evt);
            return;
        }
        const camera_io_event_t failed = { .type = CAMERA_IO_EVT_PHOTO, .size = h->config.preview_size };
        emit(h, &failed);   /* jpeg == NULL: the photo could not be taken */
    }
    h->back ^= 1;   /* the buffer just shown stays intact until the next frame */
}

static void camera_task(void *arg)
{
    camera_io_handle_t h = arg;
    while (true) {
        /* Handle every queued command first; the wait is re-evaluated each time,
         * so a START switches straight to frame processing. Idle: block on commands. */
        uint8_t cmd;
        if (xQueueReceive(h->cmds, &cmd, h->streaming ? 0 : portMAX_DELAY) == pdTRUE) {
            switch ((cmd_t)cmd) {
            case CMD_START:
                open_camera(h);
                break;
            case CMD_STOP:
                close_camera(h);
                emit_type(h, CAMERA_IO_EVT_STOPPED);
                break;
            case CMD_CAPTURE:
                h->capture_requested = h->streaming;
                break;
            case CMD_RESUME:
                h->frozen = false;
                break;
            }
            continue;
        }
        process_frame(h);   /* streaming and no command waiting; blocks up to the frame timeout */
    }
}

/* ---------- Public API ---------- */

static esp_err_t send_cmd(camera_io_handle_t h, cmd_t cmd)
{
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "null handle");
    const uint8_t c = (uint8_t)cmd;
    return xQueueSend(h->cmds, &c, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t camera_io_start(camera_io_handle_t h)
{
    return send_cmd(h, CMD_START);
}

esp_err_t camera_io_stop(camera_io_handle_t h)
{
    return send_cmd(h, CMD_STOP);
}

esp_err_t camera_io_capture(camera_io_handle_t h)
{
    return send_cmd(h, CMD_CAPTURE);
}

esp_err_t camera_io_resume(camera_io_handle_t h)
{
    return send_cmd(h, CMD_RESUME);
}

esp_err_t camera_io_create(const camera_io_config_t *config, camera_io_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config != NULL && ret_handle != NULL && config->preview_size > 0 &&
                        config->jpeg_quality > 0 && config->jpeg_quality <= 100,
                        ESP_ERR_INVALID_ARG, TAG, "bad args");
    camera_io_handle_t h = calloc(1, sizeof(*h));
    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    h->config = *config;

    const uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    const size_t raw = (size_t)config->preview_size * config->preview_size * sizeof(uint16_t);
    h->preview_bytes = (raw + BUFFER_ALIGNMENT - 1) & ~(size_t)(BUFFER_ALIGNMENT - 1);
    h->preview[0] = heap_caps_aligned_calloc(BUFFER_ALIGNMENT, 1, h->preview_bytes, caps);
    h->preview[1] = heap_caps_aligned_calloc(BUFFER_ALIGNMENT, 1, h->preview_bytes, caps);
    h->jpeg = heap_caps_malloc(JPEG_MAX_BYTES, MALLOC_CAP_SPIRAM);
    h->cmds = xQueueCreate(CMD_QUEUE_LEN, sizeof(uint8_t));

    esp_err_t err = ESP_ERR_NO_MEM;
    if (h->preview[0] == NULL || h->preview[1] == NULL || h->jpeg == NULL || h->cmds == NULL) {
        goto fail;
    }
    const ppa_client_config_t ppa_cfg = { .oper_type = PPA_OPERATION_SRM, .max_pending_trans_num = 1 };
    if ((err = ppa_register_client(&ppa_cfg, &h->ppa)) != ESP_OK) {
        goto fail;
    }
    if (xTaskCreate(camera_task, "camera_io", TASK_STACK, h, TASK_PRIO, &h->task) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    *ret_handle = h;
    return ESP_OK;

fail:
    if (h->ppa != NULL) {
        ppa_unregister_client(h->ppa);
    }
    if (h->cmds != NULL) {
        vQueueDelete(h->cmds);
    }
    heap_caps_free(h->preview[0]);
    heap_caps_free(h->preview[1]);
    heap_caps_free(h->jpeg);
    free(h);
    return err;
}
