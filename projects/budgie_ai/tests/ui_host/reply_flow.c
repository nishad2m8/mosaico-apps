/* Host checks for main/app_ui_reply.c: the reply bubble scrolls once, and a
 * real pointer tap on it stops the scroll and opens the full-reply popup. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "lvgl.h"
#include "budgie_ui.h"
#include "app_ui_internal.h"

static uint8_t fb[480 * 480 * 4] __attribute__((aligned(64)));
static int fails;
void home_mic_cb(lv_event_t *e) { (void)e; }
void home_clear_cb(lv_event_t *e) { (void)e; }
void settings_power_off_cb(lv_event_t *e) { (void)e; }

static const char *REPLY = "Mount Everest is the tallest at 8,849 meters above sea level. It sits on the "
                           "border of Nepal and China.";

/* ---- a touch pointer driven by the test ---- */
static lv_point_t s_point;
static bool s_pressed;
static void pointer_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point = s_point;
    data->state = s_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static uint32_t tick(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000); }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) { (void)a; (void)px; lv_display_flush_ready(d); }
static void run_ms(int ms) { uint32_t end = tick() + ms; while (tick() < end) { lv_timer_handler(); usleep(2000); } }
static void check(int ok, const char *what) { printf("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fails++; }
static lv_obj_t *find(lv_obj_t *root, const char *name) { return root ? lv_obj_find_by_name(root, name) : NULL; }

static void tap_at(int32_t x, int32_t y)
{
    s_point = (lv_point_t){ x, y };
    s_pressed = true;
    run_ms(80);
    s_pressed = false;
    run_ms(150);
}

static void tap(lv_obj_t *obj)
{
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    tap_at((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
}

static lv_obj_t *reply_text(void) { return find(find(home, "bubble_talking"), "text_label"); }
static bool scrolling(void) { return lv_anim_get(reply_text(), NULL) != NULL; }
static lv_obj_t *popup(void) { return find(home, "reply_popup"); }

static bool popup_shows(const char *text)
{
    lv_obj_t *p = popup();
    if (p == NULL) return false;
    lv_obj_t *card = lv_obj_get_child(p, 0);
    for (uint32_t i = 0; card != NULL && i < lv_obj_get_child_count(card); i++) {
        lv_obj_t *c = lv_obj_get_child(card, i);
        if (lv_obj_check_type(c, &lv_label_class) && strcmp(lv_label_get_text(c), text) == 0) return true;
    }
    return false;
}

int main(void)
{
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *d = lv_display_create(480, 480);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(d, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush);
    lv_indev_t *pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, pointer_read);

    budgie_ui_init("");
    lv_screen_load(home);
    ui_reply_init();
    ui_reply_set("Budgie says:", REPLY);
    lv_subject_set_int(&subject_budgie_mode, MODE_TALKING);
    run_ms(100);

    check(scrolling(), "long reply starts its scroll");
    run_ms(4000);
    tap(find(home, "bubble_talking"));
    check(!scrolling(), "tapping the bubble stops the scroll");
    check(popup() != NULL, "tapping the bubble opens the popup");
    check(popup_shows(REPLY), "popup shows the full reply");
    check(popup_shows("Budgie says:"), "popup shows the bubble title");

    /* Tap inside the card (on the text): stays open */
    lv_obj_t *card = lv_obj_get_child(popup(), 0);
    lv_area_t a;
    lv_obj_get_coords(card, &a);
    tap_at((a.x1 + a.x2) / 2, a.y2 - 40);
    check(popup() != NULL, "tapping the card keeps the popup open");

    tap(find(home, "reply_close_btn"));
    check(popup() == NULL, "close button closes the popup");
    check(!scrolling(), "bubble stays still after closing");

    tap(find(home, "bubble_talking"));
    tap_at(240, 470);   /* dimmed area below the card */
    check(popup() == NULL, "tapping outside the card closes the popup");

    ui_reply_set(NULL, "A second reply that is long enough to need scrolling inside the bubble.");
    run_ms(100);
    check(scrolling(), "a new reply scrolls again");
    run_ms(14000);
    check(!scrolling(), "the reply scrolls only once");
    tap(find(home, "bubble_talking"));
    check(popup_shows("A second reply that is long enough to need scrolling inside the bubble."),
          "popup opens after the scroll finished");
    ui_reply_set("Oops!", "Short error.");
    check(popup() == NULL, "a new reply closes an open popup");

    printf("%s (%d failures)\n", fails ? "SOME FAILED" : "ALL PASSED", fails);
    return fails ? 1 : 0;
}
