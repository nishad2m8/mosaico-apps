/* Host checks for main/app_ui_history.c: the idle bubble opens the History
 * screen, cards open the full exchange, x deletes one, Clear all (confirmed)
 * forgets everything. Driven with a pointer against a fake history store. */
#include <stdio.h>
#include <stdlib.h>
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
void app_ui_internal_toast(const char *text) { (void)text; }

/* ---- fake history store, oldest first ---- */
typedef struct { uint32_t id; const char *q; const char *a; } turn_t;
static turn_t s_turns[8];
static size_t s_count;
static int s_clears, s_deletes;

static void store_reset(void)
{
    static const turn_t init[] = {
        { 1, "How far is the moon?", "About 384,400 kilometers from Earth on average." },
        { 2, "Tell me a joke", "Why did the budgie sit on the clock? It wanted to be on time!" },
        { 3, "What is the tallest mountain?", "Mount Everest is the tallest at 8,849 meters above sea level. "
                                              "It sits on the border of Nepal and China." },
    };
    memcpy(s_turns, init, sizeof(init));
    s_count = sizeof(init) / sizeof(init[0]);
}

static size_t fake_list(chat_history_entry_t *out, size_t out_max, void *ctx)
{
    (void)ctx;
    size_t n = 0;
    for (size_t i = s_count; i > 0 && n < out_max; i--, n++) {
        out[n] = (chat_history_entry_t){ s_turns[i - 1].id, strdup(s_turns[i - 1].q), strdup(s_turns[i - 1].a) };
    }
    return n;
}
static void fake_delete(uint32_t id, void *ctx)
{
    (void)ctx;
    s_deletes++;
    for (size_t i = 0; i < s_count; i++) {
        if (s_turns[i].id == id) {
            memmove(&s_turns[i], &s_turns[i + 1], (s_count - i - 1) * sizeof(turn_t));
            s_count--;
            return;
        }
    }
}
static void fake_clear(void *ctx) { (void)ctx; s_clears++; s_count = 0; }
void chat_history_list_release(chat_history_entry_t *e, size_t n)
{
    for (size_t i = 0; i < n; i++) { free(e[i].user); free(e[i].assistant); e[i].user = e[i].assistant = NULL; }
}
static const app_ui_callbacks_t s_cb = {
    .list_history = fake_list, .on_delete_history = fake_delete, .on_clear_history = fake_clear,
};
const app_ui_callbacks_t *app_ui_internal_callbacks(void) { return &s_cb; }

/* ---- pointer + helpers ---- */
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
static lv_obj_t *active(void) { return lv_screen_active(); }
static bool on_history(void) { return find(active(), "history_list") != NULL; }

static void tap(lv_obj_t *obj)
{
    if (obj == NULL) { check(0, "tap target exists"); return; }
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    s_point = (lv_point_t){ (a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2 };
    s_pressed = true;
    run_ms(80);
    s_pressed = false;
    run_ms(400);   /* covers the 250 ms screen animation */
}

static uint32_t cards(void)
{
    lv_obj_t *list = find(active(), "history_list");
    uint32_t n = 0;
    for (uint32_t i = 0; list != NULL && i < lv_obj_get_child_count(list); i++) {
        lv_obj_t *c = lv_obj_get_child(list, i);
        n += lv_obj_get_name(c) != NULL && strcmp(lv_obj_get_name(c), "history_card") == 0;
    }
    return n;
}
static lv_obj_t *card(uint32_t index)
{
    lv_obj_t *list = find(active(), "history_list");
    return list != NULL && index < lv_obj_get_child_count(list) ? lv_obj_get_child(list, index) : NULL;
}
static const char *card_question(uint32_t index)
{
    lv_obj_t *c = card(index);
    return c != NULL ? lv_label_get_text(lv_obj_get_child(c, 0)) : "";
}
static bool popup_has(const char *text)
{
    lv_obj_t *p = find(active(), "reply_popup");
    lv_obj_t *c = p ? lv_obj_get_child(p, 0) : NULL;
    for (uint32_t i = 0; c != NULL && i < lv_obj_get_child_count(c); i++) {
        lv_obj_t *l = lv_obj_get_child(c, i);
        if (lv_obj_check_type(l, &lv_label_class) && strcmp(lv_label_get_text(l), text) == 0) return true;
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
    ui_history_init();
    store_reset();
    lv_subject_set_int(&subject_budgie_mode, MODE_IDLE);
    run_ms(100);

    tap(find(home, "bubble_idle"));
    check(on_history(), "tapping the idle bubble opens History");
    check(cards() == 3, "History lists every exchange");
    check(strcmp(card_question(0), "What is the tallest mountain?") == 0, "newest exchange is first");

    tap(card(0));
    check(popup_has("What is the tallest mountain?"), "tapping a card shows its question");
    check(popup_has(s_turns[2].a), "tapping a card shows the full answer");
    tap(find(active(), "reply_close_btn"));
    check(find(active(), "reply_popup") == NULL, "close button closes the popup");

    tap(find(card(1), "history_delete_btn"));
    check(s_deletes == 1 && s_count == 2, "x deletes that exchange from the store");
    check(cards() == 2 && strcmp(card_question(1), "How far is the moon?") == 0, "list refreshes without it");

    tap(find(active(), "history_clear_btn"));
    check(find(active(), "history_confirm") != NULL, "Clear all asks for confirmation");
    tap(find(active(), "history_cancel_btn"));
    run_ms(50);
    check(find(active(), "history_confirm") == NULL && s_clears == 0 && cards() == 2, "Cancel keeps everything");

    tap(find(active(), "history_clear_btn"));
    tap(find(active(), "history_clear_confirm_btn"));
    run_ms(50);
    check(s_clears == 1 && cards() == 0, "confirmed Clear all forgets everything");
    check(find(active(), "history_empty") != NULL, "empty state is shown");
    check(lv_obj_has_state(find(active(), "history_clear_btn"), LV_STATE_DISABLED), "Clear all is disabled when empty");

    lv_obj_t *history = active();
    tap(find(active(), "history_back_btn"));
    run_ms(50);
    check(active() == home, "Back returns home");
    check(!lv_obj_is_valid(history), "History screen is deleted after leaving");

    store_reset();
    tap(find(home, "bubble_idle"));
    check(on_history() && cards() == 3, "History opens again with fresh data");
    tap(card(0));
    check(ui_reply_popup_open(), "popup open on History");
    lv_screen_load(home);   /* e.g. a web chat sends the UI home while the popup is open */
    run_ms(100);
    check(active() == home && !ui_reply_popup_open(), "leaving with a popup open forgets the popup");

    printf("%s (%d failures)\n", fails ? "SOME FAILED" : "ALL PASSED", fails);
    return fails ? 1 : 0;
}
