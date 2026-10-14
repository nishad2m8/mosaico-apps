// SPDX-License-Identifier: Apache-2.0
//
// Single-owner design: everything below `s_ui` is touched only on the LVGL
// task (UI callbacks, LVGL timers). Other tasks talk to it through `queue`.

#include "app_ui.h"
#include "app_ui_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "app_config.h"
#include "bsp/esp_mosaico.h"
#include "budgie_chat.h"
#include "budgie_ui.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "text_ascii.h"

static const char *TAG = "app_ui";

#define QUEUE_LEN              16
#define DRAIN_PERIOD_MS        30
#define CLOCK_PERIOD_MS        1000
#define IDLE_CHECK_PERIOD_MS   500
#define SAVE_DEBOUNCE_MS       1000
#define TOAST_MS               3500
#define TEXT_MAX               4096
#define BUBBLE_TEXT_WIDTH      260
#define CLOCK_VALID_YEAR       (2024 - 1900)
#define CLOCK_TEXT_LEN         12      /* "12:59 PM" + NUL, with headroom */
#define SCREEN_ANIM_MS         250
#define TITLE_GAP              12      /* min space between the title and the clock / battery */
#define IDLE_AFTER_REPLY_MS    2500    /* talking -> idle once the reply has finished playing */

static const uint32_t s_timeout_ms[] = { 30000, 60000, 300000, 0 /* never */ };

typedef enum {
    MSG_ASSISTANT,
    MSG_WIFI,
    MSG_BATTERY,
    MSG_CONFIG,
    MSG_NOTICE,
    MSG_WAKE,
    MSG_GO_HOME,
    MSG_CAMERA,
} msg_type_t;

typedef struct {
    msg_type_t type;
    assistant_event_type_t evt;
    assistant_mode_t mode;
    int value;
    char *text;                     /* heap copy, freed after handling */
    wifi_mgr_status_t *wifi;        /* heap copy for MSG_WIFI */
    ui_camera_msg_t camera;         /* MSG_CAMERA payload */
} ui_msg_t;

static struct {
    QueueHandle_t queue;
    app_ui_callbacks_t cb;
    char *text_buf;                 /* ASCII conversion scratch (TEXT_MAX) */
    app_config_t *cfg;              /* LVGL-task copy of the settings */
    lv_timer_t *save_timer;
    lv_timer_t *idle_after_reply;   /* one-shot, paused while not counting down */
    lv_obj_t *wake_catcher;         /* full-screen touch catcher while dimmed */
    bool dimmed;
    bool applying_config;           /* suppress observer saves while syncing */
    bool assistant_busy;            /* listening/thinking: keep the screen on */
    wifi_mgr_status_t wifi;
    bool have_wifi;
} s_ui;

/* ---------- Posting (any task) ---------- */

static void post(ui_msg_t *msg)
{
    if (s_ui.queue == NULL || xQueueSend(s_ui.queue, msg, 0) != pdTRUE) {
        free(msg->text);
        free(msg->wifi);
        free(msg->camera.jpeg);
        free(msg->camera.thumb);
    }
}

void app_ui_post_assistant_event(const assistant_event_t *event)
{
    if (event == NULL) {
        return;
    }
    ui_msg_t msg = {
        .type = MSG_ASSISTANT, .evt = event->type, .mode = event->mode,
        .text = event->text != NULL ? strdup(event->text) : NULL,
    };
    post(&msg);
}

void app_ui_post_wifi(const wifi_mgr_status_t *status)
{
    ui_msg_t msg = { .type = MSG_WIFI, .wifi = malloc(sizeof(wifi_mgr_status_t)) };
    if (msg.wifi != NULL && status != NULL) {
        *msg.wifi = *status;
        post(&msg);
    } else {
        free(msg.wifi);
    }
}

void app_ui_post_battery(int percent)
{
    ui_msg_t msg = { .type = MSG_BATTERY, .value = percent };
    post(&msg);
}

void app_ui_post_config_changed(void)
{
    ui_msg_t msg = { .type = MSG_CONFIG };
    post(&msg);
}

void app_ui_post_notice(const char *text)
{
    ui_msg_t msg = { .type = MSG_NOTICE, .text = text != NULL ? strdup(text) : NULL };
    post(&msg);
}

void app_ui_post_wake(void)
{
    ui_msg_t msg = { .type = MSG_WAKE };
    post(&msg);
}

void app_ui_post_go_home(void)
{
    ui_msg_t msg = { .type = MSG_GO_HOME };
    post(&msg);
}

void app_ui_internal_post_camera(ui_camera_msg_t *camera)
{
    ui_msg_t msg = { .type = MSG_CAMERA, .camera = *camera };
    post(&msg);
}

const app_ui_callbacks_t *app_ui_internal_callbacks(void)
{
    return &s_ui.cb;
}

void app_ui_blank_display(void)
{
    if (bsp_display_lock(-1)) {
        bsp_display_brightness_set(0);
        bsp_display_unlock();
    }
}

/* ---------- Helpers (LVGL task) ---------- */

static const char *ascii(const char *text)
{
    text_to_ascii(text, s_ui.text_buf, TEXT_MAX);
    return s_ui.text_buf;
}

static lv_obj_t *bubble_label(const char *bubble, const char *label)
{
    lv_obj_t *b = home != NULL ? lv_obj_find_by_name(home, bubble) : NULL;
    return b != NULL ? lv_obj_find_by_name(b, label) : NULL;
}

static void set_bubble(const char *bubble, const char *title, const char *text)
{
    lv_obj_t *t = bubble_label(bubble, "title_label");
    lv_obj_t *x = bubble_label(bubble, "text_label");
    if (t != NULL && title != NULL) {
        lv_label_set_text(t, title);
    }
    if (x != NULL && text != NULL) {
        lv_label_set_text(x, ascii(text));
    }
}

/* Long replies would push the bubble into the buttons: one line with "..." */
static void make_single_line(const char *bubble)
{
    lv_obj_t *x = bubble_label(bubble, "text_label");
    if (x != NULL) {
        lv_obj_set_width(x, BUBBLE_TEXT_WIDTH);
        lv_obj_set_height(x, lv_font_get_line_height(lv_obj_get_style_text_font(x, LV_PART_MAIN)));
        lv_label_set_long_mode(x, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_align(x, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

static void apply_brightness(void)
{
    if (!s_ui.dimmed) {
        bsp_display_brightness_set(s_ui.cfg->brightness);
    }
}

void app_ui_internal_toast(const char *text);

static void toast(const char *text)
{
    lv_obj_t *t = lv_label_create(lv_layer_top());
    lv_label_set_text(t, ascii(text));
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(t, 400);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, font_body, LV_PART_MAIN);
    lv_obj_set_style_bg_color(t, COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_radius(t, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_all(t, 12, LV_PART_MAIN);
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_delete_delayed(t, TOAST_MS);
}

void app_ui_internal_toast(const char *text)
{
    toast(text);
}

/* ---------- Screen dimming ---------- */

static void wake(void)
{
    lv_display_trigger_activity(NULL);
    if (!s_ui.dimmed) {
        return;
    }
    s_ui.dimmed = false;
    if (s_ui.wake_catcher != NULL) {
        lv_obj_delete_async(s_ui.wake_catcher);
        s_ui.wake_catcher = NULL;
    }
    apply_brightness();
}

static void catcher_cb(lv_event_t *e)
{
    wake();   /* the first touch only wakes; it never reaches the widgets below */
}

static void dim(void)
{
    s_ui.dimmed = true;
    bsp_display_brightness_set(0);
    s_ui.wake_catcher = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_ui.wake_catcher);
    lv_obj_set_size(s_ui.wake_catcher, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(s_ui.wake_catcher, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_ui.wake_catcher, catcher_cb, LV_EVENT_PRESSED, NULL);
}

static void idle_timer_cb(lv_timer_t *t)
{
    const uint32_t limit = s_timeout_ms[s_ui.cfg->screen_timeout];
    if (s_ui.dimmed || limit == 0 || s_ui.assistant_busy) {
        return;
    }
    if (lv_display_get_inactive_time(NULL) >= limit) {
        dim();
    }
}

/* ---------- Clock ---------- */

static void fit_title(void);

static void clock_timer_cb(lv_timer_t *t)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    char buf[CLOCK_TEXT_LEN];
    if (tm_now.tm_year >= CLOCK_VALID_YEAR) {
        if (lv_subject_get_int(&subject_clock_format) == APP_CLOCK_FORMAT_12H) {
            int hour12 = tm_now.tm_hour % 12;
            snprintf(buf, sizeof(buf), "%d:%02d %s", hour12 == 0 ? 12 : hour12, tm_now.tm_min,
                     tm_now.tm_hour < 12 ? "AM" : "PM");
        } else {
            strftime(buf, sizeof(buf), "%H:%M", &tm_now);
        }
    } else {
        strlcpy(buf, "--:--", sizeof(buf));
    }
    if (strcmp(lv_subject_get_string(&subject_time), buf) != 0) {
        lv_subject_copy_string(&subject_time, buf);
        fit_title();   /* the clock's width changed */
    }
}

/* ---------- Settings <-> subjects ---------- */

static void sync_subjects_from_config(void)
{
    s_ui.applying_config = true;
    lv_subject_set_int(&subject_volume, s_ui.cfg->volume);
    lv_subject_set_int(&subject_brightness, s_ui.cfg->brightness);
    lv_subject_set_int(&subject_screen_timeout, s_ui.cfg->screen_timeout);
    lv_subject_set_int(&subject_clock_format, s_ui.cfg->clock_format);
    lv_subject_set_int(&subject_chirps, s_ui.cfg->chirps);
    lv_subject_set_int(&subject_kid_safe, s_ui.cfg->kid_safe);
    s_ui.applying_config = false;
}

/* The centred title may only use the space between the clock (left) and the
 * battery (right) groups; longer titles end in "...". Re-run when they change width. */
static void fit_title(void)
{
    lv_obj_t *title = home != NULL ? lv_obj_find_by_name(home, "title_label") : NULL;
    lv_obj_t *left = home != NULL ? lv_obj_find_by_name(home, "left") : NULL;
    lv_obj_t *right = home != NULL ? lv_obj_find_by_name(home, "right") : NULL;
    lv_obj_t *bar = left != NULL ? lv_obj_get_parent(left) : NULL;
    if (title == NULL || right == NULL || bar == NULL) {
        return;
    }
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(title, LV_SIZE_CONTENT);
    lv_obj_set_height(title, lv_font_get_line_height(lv_obj_get_style_text_font(title, LV_PART_MAIN)));
    lv_obj_update_layout(bar);
    lv_area_t bar_a, left_a, right_a;
    lv_obj_get_coords(bar, &bar_a);
    lv_obj_get_coords(left, &left_a);
    lv_obj_get_coords(right, &right_a);
    const int32_t centre = (bar_a.x1 + bar_a.x2) / 2;
    const int32_t half = LV_MIN(centre - left_a.x2, right_a.x1 - centre) - TITLE_GAP;
    const int32_t max_w = LV_MAX(half * 2, 0);
    if (lv_obj_get_width(title) > max_w) {
        lv_obj_set_width(title, max_w);
    }
}

/* Title and subtitle from the web page (labels named in status_bar.xml / home.xml) */
static void apply_labels(void)
{
    lv_obj_t *title = home != NULL ? lv_obj_find_by_name(home, "title_label") : NULL;
    lv_obj_t *subtitle = home != NULL ? lv_obj_find_by_name(home, "subtitle_label") : NULL;
    if (title != NULL) {
        lv_label_set_text(title, ascii(s_ui.cfg->title));
    }
    if (subtitle != NULL) {
        lv_label_set_text(subtitle, ascii(s_ui.cfg->subtitle));
    }
    fit_title();
}

static void save_timer_cb(lv_timer_t *t)
{
    lv_timer_pause(t);
    app_config_get(s_ui.cfg);   /* start from the latest (the web page may have changed it) */
    s_ui.cfg->volume = (uint8_t)lv_subject_get_int(&subject_volume);
    s_ui.cfg->brightness = (uint8_t)lv_subject_get_int(&subject_brightness);
    s_ui.cfg->screen_timeout = (uint8_t)lv_subject_get_int(&subject_screen_timeout);
    s_ui.cfg->clock_format = (uint8_t)lv_subject_get_int(&subject_clock_format);
    s_ui.cfg->chirps = lv_subject_get_int(&subject_chirps) != 0;
    s_ui.cfg->kid_safe = lv_subject_get_int(&subject_kid_safe) != 0;
    if (app_config_set(s_ui.cfg) != ESP_OK) {
        toast("Could not save settings");
    }
}

static void setting_observer_cb(lv_observer_t *observer, lv_subject_t *subject)
{
    if (s_ui.applying_config || s_ui.save_timer == NULL) {
        return;
    }
    if (subject == &subject_brightness) {
        s_ui.cfg->brightness = (uint8_t)lv_subject_get_int(subject);
        apply_brightness();   /* immediate feedback while dragging */
    } else if (subject == &subject_clock_format) {
        clock_timer_cb(NULL);   /* show the new format right away */
    }
    lv_timer_reset(s_ui.save_timer);
    lv_timer_resume(s_ui.save_timer);
}

/* ---------- Message handlers ---------- */

static void refresh_idle_bubble(void)
{
    char line[64];
    if (s_ui.have_wifi && s_ui.wifi.state != WIFI_MGR_STATE_CONNECTED && s_ui.wifi.ap_active) {
        snprintf(line, sizeof(line), "Join Wi-Fi %s", s_ui.wifi.ap_ssid);
        set_bubble("bubble_idle", "Let's get set up!", line);
    } else if (!app_config_ai_ready() && s_ui.have_wifi && s_ui.wifi.ip[0] != '\0') {
        snprintf(line, sizeof(line), "Add my AI key at %s", s_ui.wifi.ip);
        set_bubble("bubble_idle", "Almost ready!", line);
    } else {
        set_bubble("bubble_idle", "Hi! I'm Budgie.", "How can I help you today?");
    }
}

static void idle_after_reply_cb(lv_timer_t *t)
{
    if (ui_reply_popup_open()) {
        return;   /* still reading the full reply: check again after the next period */
    }
    lv_timer_pause(t);
    if (lv_subject_get_int(&subject_budgie_mode) == ASSISTANT_MODE_TALKING) {
        refresh_idle_bubble();
        lv_subject_set_int(&subject_budgie_mode, ASSISTANT_MODE_IDLE);
    }
}

static void handle_assistant(const ui_msg_t *msg)
{
    wake();
    switch (msg->evt) {
    case ASSISTANT_EVT_MODE:
        lv_timer_pause(s_ui.idle_after_reply);   /* a new request takes over */
        s_ui.assistant_busy = msg->mode == ASSISTANT_MODE_LISTENING ||
                              msg->mode == ASSISTANT_MODE_THINKING;
        if (msg->mode == ASSISTANT_MODE_THINKING) {
            set_bubble("bubble_thinking", "Hmm, let me think...", "One moment please.");
        }
        lv_subject_set_int(&subject_budgie_mode, msg->mode);
        break;
    case ASSISTANT_EVT_USER_TEXT: {
        char line[96];
        snprintf(line, sizeof(line), "\"%s\"", msg->text != NULL ? msg->text : "");
        set_bubble("bubble_thinking", "Hmm, let me think...", line);
        break;
    }
    case ASSISTANT_EVT_REPLY:
        ui_reply_set("Budgie says:", ascii(msg->text));
        budgie_chat_set_response(ascii(msg->text));
        break;
    case ASSISTANT_EVT_REPLY_DONE:
        lv_timer_reset(s_ui.idle_after_reply);
        lv_timer_resume(s_ui.idle_after_reply);
        break;
    case ASSISTANT_EVT_ERROR:
        s_ui.assistant_busy = false;
        ui_reply_set("Oops!", ascii(msg->text));
        budgie_chat_set_response(ascii(msg->text));
        lv_subject_set_int(&subject_budgie_mode, ASSISTANT_MODE_TALKING);
        break;
    }
}

static void handle_wifi(const wifi_mgr_status_t *st)
{
    s_ui.wifi = *st;
    s_ui.have_wifi = true;

    int level = 0;
    char name[48];
    char url[48];
    if (st->state == WIFI_MGR_STATE_CONNECTED) {
        level = st->rssi >= -60 ? 3 : (st->rssi >= -70 ? 2 : 1);
        strlcpy(name, st->ssid, sizeof(name));
        strlcpy(url, st->ip, sizeof(url));
    } else if (st->state == WIFI_MGR_STATE_CONNECTING) {
        strlcpy(name, "Connecting...", sizeof(name));
        strlcpy(url, st->ap_active ? st->ap_ip : "-", sizeof(url));
    } else {
        snprintf(name, sizeof(name), "Setup: %s", st->ap_ssid);
        strlcpy(url, st->ap_active ? st->ap_ip : "-", sizeof(url));
    }
    lv_subject_set_int(&subject_wifi_level, level);
    lv_subject_copy_string(&subject_wifi_name, ascii(name));
    lv_subject_copy_string(&subject_web_url, url);
    refresh_idle_bubble();
}

static void go_home(void)
{
    wake();
    if (home != NULL && lv_screen_active() != home) {
        lv_subject_set_int(&subject_show_support, 0);
        lv_screen_load_anim(home, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    }
}

static void drain_timer_cb(lv_timer_t *t)
{
    ui_camera_poll();
    ui_msg_t msg;
    while (xQueueReceive(s_ui.queue, &msg, 0) == pdTRUE) {
        switch (msg.type) {
        case MSG_ASSISTANT:
            handle_assistant(&msg);
            break;
        case MSG_WIFI:
            handle_wifi(msg.wifi);
            break;
        case MSG_BATTERY:
            lv_subject_set_int(&subject_battery, msg.value);
            break;
        case MSG_CONFIG:
            app_config_get(s_ui.cfg);
            sync_subjects_from_config();
            apply_brightness();
            apply_labels();
            refresh_idle_bubble();
            break;
        case MSG_NOTICE:
            wake();
            if (msg.text != NULL) {
                toast(msg.text);
            }
            break;
        case MSG_WAKE:
            wake();
            break;
        case MSG_GO_HOME:
            go_home();
            break;
        case MSG_CAMERA:
            wake();
            ui_camera_handle(&msg.camera);
            break;
        }
        free(msg.text);
        free(msg.wifi);
    }
}

/* ---------- Callbacks referenced by the LVGL Pro export ---------- */

void home_mic_cb(lv_event_t *e)
{
    if (s_ui.cb.on_mic != NULL) {
        s_ui.cb.on_mic(s_ui.cb.user_ctx);
    }
}

void home_clear_cb(lv_event_t *e)
{
    if (s_ui.cb.on_clear_recording != NULL) {
        s_ui.cb.on_clear_recording(s_ui.cb.user_ctx);
    }
}

void settings_power_off_cb(lv_event_t *e)
{
    if (s_ui.cb.on_power_off != NULL) {
        s_ui.cb.on_power_off(s_ui.cb.user_ctx);
    }
}

/* Typed chat answers are shown as text only (no speech) */
void budgie_chat_on_send(const char *text, bool with_photo)
{
    assistant_question_t q = { .text = text, .speak = false };
    if (with_photo) {
        q.jpeg = ui_camera_attached_photo(&q.jpeg_len);
    }
    const bool empty = (text == NULL || text[0] == '\0') && q.jpeg == NULL;
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (!empty && s_ui.cb.on_ask != NULL) {
        err = s_ui.cb.on_ask(&q, s_ui.cb.user_ctx);   /* copies the photo */
    }
    if (err == ESP_ERR_INVALID_STATE) {
        budgie_chat_set_response("I'm still busy with the last question. Try again in a moment.");
    } else if (err != ESP_OK) {
        budgie_chat_set_response("Type a message or attach a photo, then tap Send.");
    }
}

void budgie_chat_on_history_cleared(void)
{
    if (s_ui.cb.on_clear_history != NULL) {
        s_ui.cb.on_clear_history(s_ui.cb.user_ctx);
    }
}

/* ---------- Start ---------- */

static void build_ui(const char *app_version)
{
    budgie_ui_init("");
    lv_subject_copy_string(&subject_app_version, app_version);
    lv_subject_copy_string(&subject_time, "--:--");
    lv_subject_set_int(&subject_budgie_mode, ASSISTANT_MODE_IDLE);
    sync_subjects_from_config();
    apply_labels();

    /* The Editor export creates `chat` before `home`, so chat's Back button was
     * bound to a NULL screen; bind it now that both exist. */
    lv_obj_t *chat_back = lv_obj_find_by_name(chat, "back_btn");
    if (chat_back != NULL) {
        lv_obj_add_screen_load_event(chat_back, LV_EVENT_CLICKED, home, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT,
                                     SCREEN_ANIM_MS, 0);
    }

    make_single_line("bubble_thinking");
    make_single_line("bubble_idle");
    ui_reply_init();
    ui_history_init();

    lv_subject_t *settings[] = { &subject_volume, &subject_brightness, &subject_screen_timeout,
                                 &subject_clock_format, &subject_chirps, &subject_kid_safe };
    for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        lv_subject_add_observer(settings[i], setting_observer_cb, NULL);
    }
    s_ui.save_timer = lv_timer_create(save_timer_cb, SAVE_DEBOUNCE_MS, NULL);
    lv_timer_pause(s_ui.save_timer);
    s_ui.idle_after_reply = lv_timer_create(idle_after_reply_cb, IDLE_AFTER_REPLY_MS, NULL);
    lv_timer_pause(s_ui.idle_after_reply);
    ui_camera_init();
    lv_timer_create(drain_timer_cb, DRAIN_PERIOD_MS, NULL);
    lv_timer_create(clock_timer_cb, CLOCK_PERIOD_MS, NULL);
    lv_timer_create(idle_timer_cb, IDLE_CHECK_PERIOD_MS, NULL);

    lv_screen_load(home);
}

esp_err_t app_ui_start(const app_ui_callbacks_t *callbacks, const char *app_version)
{
    ESP_RETURN_ON_FALSE(callbacks != NULL, ESP_ERR_INVALID_ARG, TAG, "callbacks are null");
    ESP_RETURN_ON_FALSE(s_ui.queue == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");

    s_ui.cb = *callbacks;
    s_ui.queue = xQueueCreate(QUEUE_LEN, sizeof(ui_msg_t));
    s_ui.text_buf = heap_caps_malloc(TEXT_MAX, MALLOC_CAP_SPIRAM);
    s_ui.cfg = app_config_alloc();
    ESP_RETURN_ON_FALSE(s_ui.queue != NULL && s_ui.text_buf != NULL && s_ui.cfg != NULL,
                        ESP_ERR_NO_MEM, TAG, "no memory");
    app_config_get(s_ui.cfg);

    bsp_display_config_t display_cfg = BSP_DISPLAY_DEFAULT_CONFIG();
    display_cfg.enable_touch = true;
    ESP_RETURN_ON_FALSE(bsp_display_start_with_config(&display_cfg) != NULL, ESP_FAIL,
                        TAG, "display start failed");
    ESP_RETURN_ON_FALSE(bsp_display_lock(-1), ESP_ERR_TIMEOUT, TAG, "display lock");
    build_ui(app_version != NULL ? app_version : "");
    bsp_display_unlock();

    bsp_display_brightness_set(s_ui.cfg->brightness);
    ESP_LOGI(TAG, "UI ready");
    return ESP_OK;
}
