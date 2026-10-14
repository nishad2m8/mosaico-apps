/* Headless renderer for the Budgie LVGL UI: dumps screens as raw ARGB files. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lvgl.h"
#include "budgie_ui.h"
#include "budgie_chat.h"
#include "app_ui_internal.h"

#define W 480
#define H 480
static uint8_t fb[W * H * 4] __attribute__((aligned(64)));

void home_mic_cb(lv_event_t *e) { (void)e; }
void home_clear_cb(lv_event_t *e) { (void)e; }
void settings_power_off_cb(lv_event_t *e) { (void)e; }

static uint32_t tick(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000); }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) { (void)a; (void)px; lv_display_flush_ready(d); }

static void dump(const char *name)
{
    lv_refr_now(NULL);
    char path[64];
    snprintf(path, sizeof(path), "%s.raw", name);
    FILE *f = fopen(path, "wb"); fwrite(fb, 1, sizeof(fb), f); fclose(f);
    printf("wrote %s\n", path);
}

static void settle(void) { for (int i = 0; i < 40; i++) { lv_timer_handler(); } }

/* Lets real time pass so animations (the scrolling reply) move */
static void wait_ms(uint32_t ms)
{
    uint32_t end = tick() + ms;
    while ((int32_t)(end - tick()) > 0) {
        lv_timer_handler();
        nanosleep(&(struct timespec){ .tv_nsec = 5000000 }, NULL);
    }
}

/* Same as app_ui.c make_single_line() */
static void single_line(const char *bubble)
{
    lv_obj_t *b = lv_obj_find_by_name(home, bubble);
    lv_obj_t *x = lv_obj_find_by_name(b, "text_label");
    lv_obj_set_width(x, 260);
    lv_obj_set_height(x, lv_font_get_line_height(lv_obj_get_style_text_font(x, LV_PART_MAIN)));
    lv_label_set_long_mode(x, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(x, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

static void set_bubble(const char *bubble, const char *title, const char *text)
{
    lv_obj_t *b = lv_obj_find_by_name(home, bubble);
    lv_label_set_text(lv_obj_find_by_name(b, "title_label"), title);
    lv_label_set_text(lv_obj_find_by_name(b, "text_label"), text);
}

int main(void)
{
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *d = lv_display_create(W, H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(d, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush);

    budgie_ui_init("");
    lv_obj_add_flag(lv_obj_find_by_name(home, "camera_btn"), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lv_obj_find_by_name(chat, "attach_btn"), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_screen_load_event(lv_obj_find_by_name(chat, "back_btn"), LV_EVENT_CLICKED, home,
                                 LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 250, 0);   /* as in main/app_ui.c */
    lv_obj_set_width(lv_obj_find_by_name(chat, "chat_input"), 448);
    single_line("bubble_idle"); single_line("bubble_thinking"); ui_reply_init();
    lv_subject_copy_string(&subject_time, "10:24");
    lv_subject_copy_string(&subject_wifi_name, "Setup: Budgie-3F2A");
    lv_subject_copy_string(&subject_web_url, "192.168.4.1");

    lv_screen_load(home);
    set_bubble("bubble_idle", "Let's get set up!", "Join Wi-Fi Budgie-3F2A");
    settle(); dump("01_home_setup");

    lv_subject_set_int(&subject_budgie_mode, 1);
    settle(); dump("02_home_listening");

    lv_subject_set_int(&subject_budgie_mode, MODE_RECORDED);
    settle(); dump("02b_home_recorded");

    set_bubble("bubble_thinking", "Hmm, let me think...", "\"What is the tallest mountain on Earth and how tall is it?\"");
    lv_subject_set_int(&subject_budgie_mode, 2);
    settle(); dump("03_home_thinking");

    ui_reply_set("Budgie says:", "Mount Everest is the tallest at 8,849 meters above sea level. It sits on the border of Nepal and China.");
    lv_subject_set_int(&subject_budgie_mode, 3);
    settle(); dump("04_home_talking");
    wait_ms(1000); dump("04b_home_talking_paused");
    wait_ms(3000); dump("04c_home_talking_scrolling");
    wait_ms(12000); dump("04d_home_talking_done");
    lv_obj_send_event(lv_obj_find_by_name(home, "bubble_talking"), LV_EVENT_CLICKED, NULL);
    settle(); dump("04e_home_reply_popup");
    lv_obj_send_event(lv_obj_find_by_name(home, "reply_close_btn"), LV_EVENT_CLICKED, NULL);

    budgie_chat_set_response("Mount Everest is the tallest at 8,849 meters above sea level. It sits on the border of Nepal and China. Climbers usually need about two months to reach the top.");
    lv_screen_load(chat);
    settle(); dump("05_chat");

    lv_obj_t *s = settings_create();
    lv_screen_load(s);
    settle(); dump("06_settings_top");
    lv_obj_scroll_to_view(lv_obj_find_by_name(s, "display_card"), LV_ANIM_OFF);
    settle(); dump("06b_settings_display");
    lv_obj_t *list = lv_obj_find_by_name(s, "list");
    lv_obj_scroll_to_view(lv_obj_find_by_name(s, "device_card"), LV_ANIM_OFF);
    settle(); dump("07_settings_device");
    lv_obj_scroll_to_y(list, LV_COORD_MAX, LV_ANIM_OFF);
    settle(); dump("08_settings_bottom");
    return 0;
}
