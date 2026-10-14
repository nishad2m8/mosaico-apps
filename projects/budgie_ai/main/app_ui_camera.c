// SPDX-License-Identifier: Apache-2.0
//
// Camera screen behavior for the LVGL Pro "camera" screen.
//
// The Editor export only toggles subjects (photo taken, origin) and navigates;
// this file adds the real camera: a live preview canvas in "viewfinder", the
// shutter/delete/send actions, and the photo hand-off to the assistant.
//
// Ownership: `s_cam` belongs to the LVGL task, except `canvas` which the
// camera task also reads, always under the display lock. One photo slot:
// it is sent from home, or attached to the chat message (thumbnail in the
// chat's photo tile) until that message is sent or the photo is removed.

#include <stdlib.h>
#include <string.h>
#include "app_ui_internal.h"
#include "bsp/esp_mosaico.h"
#include "budgie_ui.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "ui_camera";

#define PREVIEW_LOCK_TIMEOUT_MS  100
#define SCREEN_ANIM_MS           200
#define MSG_NO_CAMERA  "No camera found. Attach the camera module to the left slot."
#define MSG_CAMERA_ERR "The camera stopped working. Try again."
#define MSG_PHOTO_ERR  "Could not take the photo. Try again."
#define MSG_BUSY       "One moment, I'm still thinking..."

static struct {
    camera_io_handle_t camera;
    lv_obj_t *screen;            /* active camera screen, NULL when not shown */
    lv_obj_t *canvas;            /* preview canvas; shared with the camera task */
    lv_obj_t *last_active;       /* screen seen by the previous poll */
    uint8_t *photo;              /* JPEG of the current photo slot */
    size_t photo_len;
    uint16_t *thumb;
    bool attached;               /* photo belongs to the chat message */
    lv_image_dsc_t thumb_dsc;
} s_cam;

/* ---------- Camera task -> UI ---------- */

void app_ui_attach_camera(camera_io_handle_t camera)
{
    s_cam.camera = camera;
}

static uint16_t *make_thumb(const uint16_t *rgb565, uint16_t size)
{
    uint16_t *t = heap_caps_malloc(UI_CAMERA_THUMB_SIZE * UI_CAMERA_THUMB_SIZE * sizeof(uint16_t),
                                   MALLOC_CAP_SPIRAM);
    if (t == NULL || rgb565 == NULL) {
        free(t);
        return NULL;
    }
    for (int y = 0; y < UI_CAMERA_THUMB_SIZE; y++) {
        for (int x = 0; x < UI_CAMERA_THUMB_SIZE; x++) {
            t[y * UI_CAMERA_THUMB_SIZE + x] =
                rgb565[(y * size / UI_CAMERA_THUMB_SIZE) * size + x * size / UI_CAMERA_THUMB_SIZE];
        }
    }
    return t;
}

/* Runs on the camera task */
void app_ui_camera_event_cb(const camera_io_event_t *evt, void *user_ctx)
{
    if (evt->type == CAMERA_IO_EVT_FRAME) {
        /* Preview frames go straight to the canvas; nothing in the LVGL task
         * ever waits for the camera task, so taking the lock here is safe. */
        if (bsp_display_lock(PREVIEW_LOCK_TIMEOUT_MS)) {
            if (s_cam.canvas != NULL) {
                lv_canvas_set_buffer(s_cam.canvas, (void *)evt->rgb565, evt->size, evt->size,
                                     LV_COLOR_FORMAT_RGB565);
                lv_obj_invalidate(s_cam.canvas);
            }
            bsp_display_unlock();
        }
        return;
    }
    ui_camera_msg_t msg = { .type = evt->type };
    if (evt->type == CAMERA_IO_EVT_PHOTO && evt->jpeg != NULL) {
        msg.jpeg = heap_caps_malloc(evt->jpeg_len, MALLOC_CAP_SPIRAM);
        msg.thumb = make_thumb(evt->rgb565, evt->size);
        if (msg.jpeg != NULL) {
            memcpy(msg.jpeg, evt->jpeg, evt->jpeg_len);
            msg.jpeg_len = evt->jpeg_len;
        }
    }
    app_ui_internal_post_camera(&msg);
}

/* ---------- Photo slot (LVGL task) ---------- */

static void drop_photo(void)
{
    free(s_cam.photo);
    free(s_cam.thumb);
    s_cam.photo = NULL;
    s_cam.thumb = NULL;
    s_cam.photo_len = 0;
    s_cam.attached = false;
    lv_obj_t *thumb = chat != NULL ? lv_obj_find_by_name(chat, "photo_thumb") : NULL;
    if (thumb != NULL) {
        lv_image_set_src(thumb, img_icon_photo);   /* back to the placeholder icon */
    }
}

const uint8_t *ui_camera_attached_photo(size_t *len)
{
    if (!s_cam.attached || s_cam.photo == NULL) {
        return NULL;
    }
    *len = s_cam.photo_len;
    return s_cam.photo;
}

static void show_chat_thumbnail(void)
{
    lv_obj_t *thumb = chat != NULL ? lv_obj_find_by_name(chat, "photo_thumb") : NULL;
    if (thumb == NULL || s_cam.thumb == NULL) {
        return;
    }
    s_cam.thumb_dsc = (lv_image_dsc_t) {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = UI_CAMERA_THUMB_SIZE,
            .h = UI_CAMERA_THUMB_SIZE,
            .stride = UI_CAMERA_THUMB_SIZE * sizeof(uint16_t),
        },
        .data_size = UI_CAMERA_THUMB_SIZE * UI_CAMERA_THUMB_SIZE * sizeof(uint16_t),
        .data = (const uint8_t *)s_cam.thumb,
    };
    lv_image_cache_drop(&s_cam.thumb_dsc);
    lv_image_set_src(thumb, &s_cam.thumb_dsc);
}

/* Chat removed the attachment (x badge) or sent it */
static void chat_photo_observer(lv_observer_t *observer, lv_subject_t *subject)
{
    if (lv_subject_get_int(subject) == 0 && s_cam.attached) {
        drop_photo();
    }
}

/* ---------- Camera screen actions (LVGL task) ---------- */

static void leave_camera(void)
{
    lv_obj_t *target = lv_subject_get_int(&subject_camera_origin) == ORIGIN_CHAT ? chat : home;
    lv_subject_set_int(&subject_photo_taken, 0);
    if (target != NULL && lv_screen_active() != target) {
        lv_screen_load_anim(target, LV_SCREEN_LOAD_ANIM_FADE_IN, SCREEN_ANIM_MS, 0, false);
    }
}

static void shutter_cb(lv_event_t *e)
{
    if (s_cam.photo == NULL || s_cam.attached) {
        camera_io_capture(s_cam.camera);
    }
}

static void delete_cb(lv_event_t *e)
{
    if (!s_cam.attached) {
        drop_photo();
    }
    camera_io_resume(s_cam.camera);
}

/* Home: ask about the photo right away (spoken answer, like a voice question) */
static void send_home_cb(lv_event_t *e)
{
    const app_ui_callbacks_t *cb = app_ui_internal_callbacks();
    if (s_cam.photo == NULL || cb->on_ask == NULL) {
        return;
    }
    const assistant_question_t q = { .jpeg = s_cam.photo, .jpeg_len = s_cam.photo_len, .speak = true };
    if (cb->on_ask(&q, cb->user_ctx) != ESP_OK) {
        lv_subject_set_int(&subject_budgie_mode, ASSISTANT_MODE_IDLE);
        app_ui_internal_toast(MSG_BUSY);
    }
    drop_photo();   /* assistant_ask() keeps its own copy */
}

/* Chat: attach to the message being typed */
static void send_chat_cb(lv_event_t *e)
{
    if (s_cam.photo != NULL) {
        s_cam.attached = true;
        show_chat_thumbnail();
    }
}

static void canvas_deleted_cb(lv_event_t *e)
{
    /* The screen is gone: stop drawing frames into it. This runs on the LVGL
     * task, which already holds the display lock the camera task takes. */
    s_cam.canvas = NULL;
}

static void add_click(lv_obj_t *screen, const char *name, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_obj_find_by_name(screen, name);
    if (btn != NULL) {
        /* Added after the export's own events, so subjects are already updated */
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    }
}

static void enter_camera(lv_obj_t *screen)
{
    s_cam.screen = screen;
    lv_obj_t *viewfinder = lv_obj_find_by_name(screen, "viewfinder");
    lv_obj_t *canvas = lv_canvas_create(viewfinder);
    lv_obj_center(canvas);
    lv_obj_add_event_cb(canvas, canvas_deleted_cb, LV_EVENT_DELETE, NULL);
    s_cam.canvas = canvas;   /* LVGL task holds the display lock here */

    add_click(screen, "shutter_btn", shutter_cb);
    add_click(screen, "delete_btn", delete_cb);
    add_click(screen, "send_btn", send_home_cb);
    add_click(screen, "send_btn_chat", send_chat_cb);

    if (s_cam.camera == NULL || camera_io_start(s_cam.camera) != ESP_OK) {
        app_ui_internal_toast(MSG_NO_CAMERA);
        leave_camera();
    }
    ESP_LOGI(TAG, "camera screen opened");
}

static void exit_camera(void)
{
    s_cam.screen = NULL;
    if (s_cam.camera != NULL) {
        camera_io_stop(s_cam.camera);
    }
    if (!s_cam.attached) {
        drop_photo();
    }
}

void ui_camera_poll(void)
{
    lv_obj_t *active = lv_screen_active();
    if (active == s_cam.last_active) {
        return;
    }
    s_cam.last_active = active;
    if (s_cam.screen != NULL && active != s_cam.screen) {
        exit_camera();
    }
    if (active != NULL && s_cam.screen == NULL && lv_obj_find_by_name(active, "viewfinder") != NULL) {
        enter_camera(active);
    }
}

void ui_camera_handle(ui_camera_msg_t *msg)
{
    switch (msg->type) {
    case CAMERA_IO_EVT_NOT_FOUND:
        app_ui_internal_toast(MSG_NO_CAMERA);
        if (s_cam.screen != NULL) {
            leave_camera();
        }
        break;
    case CAMERA_IO_EVT_ERROR:
        app_ui_internal_toast(MSG_CAMERA_ERR);
        if (s_cam.screen != NULL) {
            leave_camera();
        }
        break;
    case CAMERA_IO_EVT_PHOTO:
        if (msg->jpeg == NULL || s_cam.screen == NULL) {
            if (s_cam.screen != NULL) {
                app_ui_internal_toast(MSG_PHOTO_ERR);
                lv_subject_set_int(&subject_photo_taken, 0);
                camera_io_resume(s_cam.camera);
            }
            break;
        }
        if (s_cam.attached) {
            /* A new photo from the chat replaces the attached one */
            drop_photo();
        }
        free(s_cam.photo);
        free(s_cam.thumb);
        s_cam.photo = msg->jpeg;
        s_cam.photo_len = msg->jpeg_len;
        s_cam.thumb = msg->thumb;
        msg->jpeg = NULL;
        msg->thumb = NULL;
        break;
    default:
        break;
    }
    free(msg->jpeg);
    free(msg->thumb);
}

void ui_camera_init(void)
{
    s_cam.last_active = lv_screen_active();
    lv_subject_add_observer(&subject_chat_has_photo, chat_photo_observer, NULL);
}
