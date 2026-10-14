/* Host checks for main/app_ui_camera.c: the real camera screen logic against a
 * fake camera (camera_io_* stubs) and fake camera events. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "lvgl.h"
#include "budgie_ui.h"
#include "app_ui_internal.h"

static uint8_t fb[480 * 480 * 4] __attribute__((aligned(64)));
static int fails;
static int n_start, n_stop, n_capture, n_resume, n_ask;
static assistant_question_t last_q;
static size_t last_q_len;
static char last_toast[128];

/* ---- stubs for the firmware-side dependencies ---- */
bool bsp_display_lock(int timeout_ms) { (void)timeout_ms; return true; }
void bsp_display_unlock(void) {}
esp_err_t camera_io_start(camera_io_handle_t h) { (void)h; n_start++; return ESP_OK; }
esp_err_t camera_io_stop(camera_io_handle_t h) { (void)h; n_stop++; return ESP_OK; }
esp_err_t camera_io_capture(camera_io_handle_t h) { (void)h; n_capture++; return ESP_OK; }
esp_err_t camera_io_resume(camera_io_handle_t h) { (void)h; n_resume++; return ESP_OK; }
void app_ui_internal_toast(const char *text) { snprintf(last_toast, sizeof(last_toast), "%s", text); }
void app_ui_internal_post_camera(ui_camera_msg_t *msg) { ui_camera_handle(msg); }
static esp_err_t fake_ask(const assistant_question_t *q, void *ctx)
{
    (void)ctx;
    n_ask++;
    last_q = *q;
    last_q_len = q->jpeg_len;
    return ESP_OK;
}
static const app_ui_callbacks_t s_cb = { .on_ask = fake_ask };
const app_ui_callbacks_t *app_ui_internal_callbacks(void) { return &s_cb; }
void home_mic_cb(lv_event_t *e) { (void)e; }
void home_clear_cb(lv_event_t *e) { (void)e; }
void settings_power_off_cb(lv_event_t *e) { (void)e; }

/* ---- helpers ---- */
static uint32_t tick(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000); }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) { (void)a; (void)px; lv_display_flush_ready(d); }
static void run_ms(int ms)
{
    uint32_t end = tick() + ms;
    while (tick() < end) { lv_timer_handler(); ui_camera_poll(); usleep(2000); }
}
static void click(lv_obj_t *o) { lv_obj_send_event(o, LV_EVENT_CLICKED, NULL); run_ms(450); }
static void check(int ok, const char *what) { printf("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fails++; }
static lv_obj_t *find(lv_obj_t *root, const char *name) { return root ? lv_obj_find_by_name(root, name) : NULL; }
static bool on_camera(void) { return find(lv_screen_active(), "viewfinder") != NULL; }

static uint16_t preview[480 * 480];
static const uint8_t fake_jpeg[] = { 0xFF, 0xD8, 0xFF, 0xE0, 1, 2, 3, 4, 5, 0xFF, 0xD9 };

static void camera_event(camera_io_event_type_t type)
{
    camera_io_event_t evt = { .type = type, .rgb565 = preview, .size = 480 };
    if (type == CAMERA_IO_EVT_PHOTO) {
        evt.jpeg = fake_jpeg;
        evt.jpeg_len = sizeof(fake_jpeg);
    }
    app_ui_camera_event_cb(&evt, NULL);
    run_ms(50);
}

static void dump(const char *path)
{
    lv_refr_now(NULL);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(fb, 1, sizeof(fb), f); fclose(f); }
}

int main(int argc, char **argv)
{
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *d = lv_display_create(480, 480);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(d, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush);
    budgie_ui_init("");
    lv_screen_load(home);
    ui_camera_init();
    app_ui_attach_camera((camera_io_handle_t)1);
    for (int y = 0; y < 480; y++)            /* fake "scene": vertical color bands */
        for (int x = 0; x < 480; x++)
            preview[y * 480 + x] = (uint16_t)(((x / 60) & 1) ? 0x07E0 : 0x001F) | (uint16_t)((y / 4) << 11);

    /* No module: back home with an explanation */
    click(find(home, "camera_btn"));
    check(on_camera() && n_start == 1, "home camera button opens camera screen and starts the camera");
    camera_event(CAMERA_IO_EVT_NOT_FOUND);
    run_ms(300);
    check(lv_screen_active() == home, "no module -> returns home");
    check(strstr(last_toast, "left slot") != NULL, "no module -> toast explains the left slot");
    check(n_stop == 1, "leaving the camera screen stops the camera");

    /* Preview + photo + send from home */
    click(find(home, "camera_btn"));
    camera_event(CAMERA_IO_EVT_STARTED);
    camera_event(CAMERA_IO_EVT_FRAME);
    lv_obj_t *vf = find(lv_screen_active(), "viewfinder");
    lv_obj_t *canvas = vf ? lv_obj_get_child(vf, 0) : NULL;
    check(canvas != NULL && lv_canvas_get_draw_buf(canvas) != NULL, "preview frame lands on the viewfinder canvas");
    if (argc > 1) dump(argv[1]);
    click(find(lv_screen_active(), "shutter_btn"));
    check(n_capture == 1 && lv_subject_get_int(&subject_photo_taken) == 1, "shutter requests a capture");
    camera_event(CAMERA_IO_EVT_PHOTO);
    click(find(lv_screen_active(), "send_btn"));
    run_ms(300);
    check(n_ask == 1 && last_q.jpeg != NULL && last_q_len == sizeof(fake_jpeg) && last_q.speak,
          "home Send asks about the photo with a spoken answer");
    check(lv_screen_active() == home && lv_subject_get_int(&subject_budgie_mode) == ASSISTANT_MODE_THINKING,
          "home Send returns home thinking");

    /* Delete re-arms the preview */
    click(find(home, "camera_btn"));
    click(find(lv_screen_active(), "shutter_btn"));
    camera_event(CAMERA_IO_EVT_PHOTO);
    click(find(lv_screen_active(), "delete_btn"));
    check(n_resume >= 1 && lv_subject_get_int(&subject_photo_taken) == 0, "delete resumes the live preview");
    click(find(lv_screen_active(), "back_btn"));
    run_ms(300);
    check(lv_screen_active() == home, "camera Back returns home");

    /* Attach from chat, then remove it */
    lv_screen_load(chat);
    run_ms(50);
    click(find(chat, "attach_btn"));
    check(on_camera() && lv_subject_get_int(&subject_camera_origin) == ORIGIN_CHAT, "chat camera button opens camera (origin chat)");
    click(find(lv_screen_active(), "shutter_btn"));
    camera_event(CAMERA_IO_EVT_PHOTO);
    click(find(lv_screen_active(), "send_btn_chat"));
    run_ms(300);
    size_t len = 0;
    const uint8_t *att = ui_camera_attached_photo(&len);
    check(lv_screen_active() == chat && lv_subject_get_int(&subject_chat_has_photo) == 1 && att != NULL &&
          len == sizeof(fake_jpeg), "chat Send attaches the photo to the message");
    const lv_image_dsc_t *src = lv_image_get_src(find(chat, "photo_thumb"));
    check(src != NULL && src->header.w == UI_CAMERA_THUMB_SIZE, "chat photo tile shows the thumbnail");
    click(find(chat, "photo_remove_btn"));
    check(ui_camera_attached_photo(&len) == NULL, "x badge removes the attached photo");

    printf("%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
