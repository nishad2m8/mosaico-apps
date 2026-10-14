// SPDX-License-Identifier: Apache-2.0
//
// Camera preview + photo capture for the Mosaico camera module (OV3640 /
// SC101IOT, left slot only), built on the BSP's mosaico_module_camera.
//
// A dedicated task owns the camera. Every public call only queues a request
// and returns immediately, so callers (including the LVGL task) never block
// on discovery, frames or JPEG encoding. Results arrive through the event
// callback, which runs on the camera task.
//
// Preview frames are square RGB565 (center crop of the sensor, rotated
// upright) in two PSRAM buffers owned by the instance. A FRAME event's buffer
// stays untouched until the next FRAME event; after PHOTO the preview is
// frozen (and its buffer kept) until camera_io_resume() or camera_io_stop().
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct camera_io_t *camera_io_handle_t;

typedef enum {
    CAMERA_IO_EVT_STARTED,       /* streaming */
    CAMERA_IO_EVT_NOT_FOUND,     /* no camera module in the left slot */
    CAMERA_IO_EVT_ERROR,         /* open/stream failure; camera closed */
    CAMERA_IO_EVT_FRAME,         /* new preview frame in `rgb565` */
    CAMERA_IO_EVT_PHOTO,         /* photo taken: `jpeg` + frozen `rgb565`; jpeg NULL = failed */
    CAMERA_IO_EVT_STOPPED,
} camera_io_event_type_t;

typedef struct {
    camera_io_event_type_t type;
    const uint16_t *rgb565;      /* FRAME/PHOTO: size x size pixels */
    uint16_t size;
    const uint8_t *jpeg;         /* PHOTO: valid only during the callback */
    size_t jpeg_len;
} camera_io_event_t;

typedef void (*camera_io_event_cb_t)(const camera_io_event_t *event, void *user_ctx);

typedef struct {
    uint16_t preview_size;       /* square preview edge in pixels */
    uint8_t jpeg_quality;        /* 1..100 */
    camera_io_event_cb_t event_cb;
    void *user_ctx;
} camera_io_config_t;

/** Allocate buffers and start the (idle) camera task. Does not touch the camera. */
esp_err_t camera_io_create(const camera_io_config_t *config, camera_io_handle_t *ret_handle);

/** Find, open and stream the camera module. */
esp_err_t camera_io_start(camera_io_handle_t handle);

/** Stop streaming and release the camera module. */
esp_err_t camera_io_stop(camera_io_handle_t handle);

/** Take a photo from the next preview frame and freeze the preview. */
esp_err_t camera_io_capture(camera_io_handle_t handle);

/** Unfreeze the preview after a photo. */
esp_err_t camera_io_resume(camera_io_handle_t handle);

#ifdef __cplusplus
}
#endif
