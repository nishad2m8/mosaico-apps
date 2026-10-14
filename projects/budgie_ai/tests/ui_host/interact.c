/* Interaction checks for the Budgie LVGL UI: click real widgets via events. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "lvgl.h"
#include "budgie_ui.h"
#include "budgie_chat.h"

static uint8_t fb[480 * 480 * 4] __attribute__((aligned(64)));
static int mic_calls, clear_calls, power_calls, fails;
void home_mic_cb(lv_event_t *e) { (void)e; mic_calls++; }
void home_clear_cb(lv_event_t *e) { (void)e; clear_calls++; }
void settings_power_off_cb(lv_event_t *e) { (void)e; power_calls++; }

static uint32_t tick(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000); }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) { (void)a; (void)px; lv_display_flush_ready(d); }
static void run_ms(int ms) { uint32_t end = tick() + ms; while (tick() < end) { lv_timer_handler(); usleep(2000); } }
static void click(lv_obj_t *o) { lv_obj_send_event(o, LV_EVENT_PRESSED, NULL); lv_obj_send_event(o, LV_EVENT_RELEASED, NULL); lv_obj_send_event(o, LV_EVENT_CLICKED, NULL); run_ms(400); }
static void check(int ok, const char *what) { printf("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fails++; }
static lv_obj_t *find(lv_obj_t *root, const char *name) { return root ? lv_obj_find_by_name(root, name) : NULL; }
/* settings_option buttons are unnamed: find the one whose option_label shows `text` */
static lv_obj_t *find_option(lv_obj_t *root, const char *text)
{
    if (root == NULL) return NULL;
    lv_obj_t *label = lv_obj_get_child_by_name(root, "option_label");
    if (label != NULL && strcmp(lv_label_get_text(label), text) == 0) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *hit = find_option(lv_obj_get_child(root, i), text);
        if (hit != NULL) return hit;
    }
    return NULL;
}

/* Find the n-th t9_key child with the given letters label */
static lv_obj_t *t9(const char *letters)
{
    lv_obj_t *pad = find(chat, "keypad");
    for (uint32_t i = 0; i < lv_obj_get_child_count(pad); i++) {
        lv_obj_t *k = lv_obj_get_child(pad, i);
        lv_obj_t *l = find(k, "letters_label");
        if (l && strcmp(lv_label_get_text(l), letters) == 0) return k;
    }
    return NULL;
}

int main(void)
{
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *d = lv_display_create(480, 480);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(d, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush);
    budgie_ui_init("");
    /* Same runtime fix as main/app_ui.c */
    lv_obj_add_screen_load_event(find(chat, "back_btn"), LV_EVENT_CLICKED, home, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 250, 0);
    lv_screen_load(home);
    run_ms(100);

    click(find(home, "mic_btn"));
    check(mic_calls == 1, "home mic button calls home_mic_cb (no preview mode stepping)");
    check(lv_subject_get_int(&subject_budgie_mode) == 0, "mic tap alone leaves mode to the app");

    /* Recorded state: mic shows Send, red Clear appears and reports taps */
    lv_obj_t *clear_btn = find(home, "clear_btn");
    lv_obj_t *send_icon = find(home, "send");
    check(clear_btn != NULL && lv_obj_has_flag(clear_btn, LV_OBJ_FLAG_HIDDEN), "Clear hidden while idle");
    lv_subject_set_int(&subject_budgie_mode, MODE_LISTENING);
    check(lv_obj_has_flag(clear_btn, LV_OBJ_FLAG_HIDDEN) && !lv_obj_has_flag(find(home, "stop"), LV_OBJ_FLAG_HIDDEN),
          "listening shows Stop, no Clear");
    check(lv_obj_has_flag(find(home, "mic_icon"), LV_OBJ_FLAG_HIDDEN), "listening hides the mic icon under Stop");
    lv_subject_set_int(&subject_budgie_mode, MODE_RECORDED);
    run_ms(50);
    check(!lv_obj_has_flag(clear_btn, LV_OBJ_FLAG_HIDDEN), "recorded shows red Clear");
    check(send_icon != NULL && !lv_obj_has_flag(send_icon, LV_OBJ_FLAG_HIDDEN) &&
          lv_obj_has_flag(find(home, "stop"), LV_OBJ_FLAG_HIDDEN), "recorded mic shows Send");
    lv_obj_t *mic_img = lv_obj_get_child(find(home, "mic_icon"), 0);
    check(!lv_obj_is_visible(mic_img), "recorded hides the mic icon under Send");
    check(lv_obj_get_parent(clear_btn) == home, "Clear sits on the bubble, not in the button row");
    check(!lv_obj_has_flag(find(home, "bubble_recorded"), LV_OBJ_FLAG_HIDDEN), "recorded bubble says Tap to send");
    click(clear_btn);
    check(clear_calls == 1, "Clear calls home_clear_cb");
    click(find(home, "mic_btn"));
    check(mic_calls == 2, "Send (mic) calls home_mic_cb");
    lv_subject_set_int(&subject_budgie_mode, MODE_IDLE);

    click(find(home, "keyboard_btn"));
    check(lv_screen_active() == chat, "keyboard button opens chat");

    /* Type "hi" with T9: 4 4 (pause) 4 4 4 -> h i */
    lv_obj_t *k4 = t9("ghi");
    click(k4); run_ms(10); lv_obj_send_event(k4, LV_EVENT_CLICKED, NULL);   /* g -> h (multi-tap) */
    run_ms(1000);
    click(k4); lv_obj_send_event(k4, LV_EVENT_CLICKED, NULL); lv_obj_send_event(k4, LV_EVENT_CLICKED, NULL);
    const char *typed = lv_textarea_get_text(find(chat, "chat_input"));
    check(strcmp(typed, "hi") == 0, "T9 multi-tap types \"hi\"");
    printf("      typed=\"%s\"\n", typed);

    click(find(chat, "send_key"));
    const char *resp = lv_label_get_text(find(chat, "response"));
    check(strstr(resp, "hi") != NULL, "Send passes the text to budgie_chat_on_send (echo stub)");
    check(lv_textarea_get_text(find(chat, "chat_input"))[0] == '\0', "Send clears the input");

    click(find(chat, "back_btn"));
    run_ms(400);
    check(lv_screen_active() == home, "chat Back returns home (export ordering bug fixed)");

    lv_obj_t *gear = NULL;
    lv_obj_t *bar = lv_obj_get_child(home, 0);
    gear = find(bar, "settings");
    click(gear);
    lv_obj_t *settings_scr = lv_screen_active();
    check(settings_scr != home && find(settings_scr, "power_off_btn") != NULL, "gear opens Settings with Power off row");
    check(strcmp(lv_label_get_text(find(settings_scr, "power_off_label")), "Power off") == 0, "Power off label");

    click(find(settings_scr, "power_off_btn"));
    check(power_calls == 1, "Power off calls settings_power_off_cb");

    lv_subject_copy_string(&subject_web_url, "192.168.1.42");
    run_ms(50);
    check(lv_subject_get_int(&subject_volume) == 70, "volume subject default 70");
    check(lv_subject_get_int(&subject_clock_format) == 1, "clock format default 24 hr");
    click(find_option(settings_scr, "12 hr"));
    check(lv_subject_get_int(&subject_clock_format) == 0, "12 hr option sets clock format");
    click(find_option(settings_scr, "24 hr"));
    check(lv_subject_get_int(&subject_clock_format) == 1, "24 hr option sets clock format");

    click(find(settings_scr, "clear_history_btn"));
    check(lv_obj_has_state(find(settings_scr, "clear_history_btn"), LV_STATE_DISABLED), "Clear history disables itself");

    click(find(settings_scr, "back_btn"));
    run_ms(400);
    check(lv_screen_active() == home, "Settings Back returns home (settings screen deleted)");

    click(find(home, "keyboard_btn"));
    check(strstr(lv_label_get_text(find(chat, "response")), "Type your message") != NULL,
          "chat resets to greeting after Clear history");

    printf("%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
