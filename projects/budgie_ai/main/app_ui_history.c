// SPDX-License-Identifier: Apache-2.0
//
// History screen: tapping the idle bubble on home opens a newest-first list of
// the conversation. A card opens the full exchange in the reply popup, its x
// deletes that exchange, and "Clear all" (after a confirm dialog) forgets all.
//
// Single owner: everything here runs on the LVGL task. The screen is built on
// open and deleted when it is unloaded; the conversation itself stays with the
// assistant and is read through the app_ui callbacks.

#include "app_ui_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_config.h"
#include "budgie_ui.h"
#include "esp_heap_caps.h"
#include "text_ascii.h"

#define IDLE_BUBBLE            "bubble_idle"
#define HISTORY_MAX            APP_CONFIG_HISTORY_TURNS_MAX
#define TEXT_MAX               4096
#define SCREEN_ANIM_MS         250
#define HEADER_BTN_SIZE        44
#define HEADER_BTN_X           8
#define HEADER_BTN_Y           6
#define TITLE_Y                16
#define CLEAR_BTN_HEIGHT       36
#define CLEAR_BTN_X            (-12)
#define CLEAR_BTN_Y            10
#define CLEAR_BTN_PAD_HOR      14
#define LIST_X                 16
#define LIST_Y                 64
#define LIST_W                 448
#define LIST_GAP               10
#define CARD_RADIUS            20
#define CARD_PAD               14
#define CARD_PAD_RIGHT         56      /* room for the delete button */
#define CARD_ROW_GAP           4
#define ANSWER_LINES           2
#define TEXT_LINE_SPACE        4
#define DELETE_SIZE            36
#define DELETE_X               44      /* into the card's right padding */
/* Empty state, relative to the screen centre (the list is centred LIST_Y / 2 lower) */
#define EMPTY_AVATAR_Y         (-40)
#define EMPTY_TITLE_Y          20
#define EMPTY_HINT_Y           52
#define DIALOG_WIDTH           380
#define DIALOG_RADIUS          24
#define DIALOG_PAD             24
#define DIALOG_ROW_GAP         10
#define DIALOG_BTN_HEIGHT      48
#define DIALOG_BTN_GAP         12
#define DIALOG_BTN_TOP         8
#define DIALOG_MSG_LEN         64

static struct {
    lv_obj_t *screen;            /* NULL while the History screen is not shown */
    lv_obj_t *list;
    lv_obj_t *clear_btn;
    char *text_buf;              /* ASCII conversion scratch, lives with the screen */
    chat_history_entry_t entries[HISTORY_MAX];
    size_t count;                /* entries shown; their strings are owned here */
} s_hist;

static const char *ascii(const char *text)
{
    text_to_ascii(text, s_hist.text_buf, TEXT_MAX);
    return s_hist.text_buf;
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

static void release_entries(void)
{
    chat_history_list_release(s_hist.entries, s_hist.count);
    s_hist.count = 0;
}

/* ---------- List ---------- */

static void rebuild(void);

static void rebuild_async_cb(void *arg)
{
    (void)arg;
    rebuild();
}

static const chat_history_entry_t *entry_of(lv_event_t *e)
{
    const size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    return i < s_hist.count ? &s_hist.entries[i] : NULL;
}

static void card_clicked_cb(lv_event_t *e)
{
    const chat_history_entry_t *entry = entry_of(e);
    if (entry == NULL) {
        return;
    }
    /* ascii() shares one buffer, so convert the question into a copy first */
    char *question = strdup(ascii(entry->user));
    if (question != NULL) {
        ui_reply_show_popup(s_hist.screen, question, ascii(entry->assistant));
        free(question);
    }
}

static void delete_clicked_cb(lv_event_t *e)
{
    const chat_history_entry_t *entry = entry_of(e);
    const app_ui_callbacks_t *cb = app_ui_internal_callbacks();
    if (entry != NULL && cb->on_delete_history != NULL) {
        cb->on_delete_history(entry->id, cb->user_ctx);
    }
    lv_async_call(rebuild_async_cb, NULL);   /* not while the clicked button is handling its event */
}

static void add_card(size_t index)
{
    const chat_history_entry_t *entry = &s_hist.entries[index];
    void *user_data = (void *)(uintptr_t)index;

    lv_obj_t *card = lv_obj_create(s_hist.list);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, COLOR_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, COLOR_KEY_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(card, COLOR_BUBBLE_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, CARD_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_right(card, CARD_PAD_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, CARD_ROW_GAP, LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(card, card_clicked_cb, LV_EVENT_CLICKED, user_data);
    lv_obj_set_name(card, "history_card");

    lv_obj_t *question = add_label(card, ascii(entry->user), font_bubble_title, COLOR_TEXT);
    lv_obj_set_width(question, lv_pct(100));
    lv_obj_set_height(question, lv_font_get_line_height(font_bubble_title));
    lv_label_set_long_mode(question, LV_LABEL_LONG_MODE_DOTS);

    lv_obj_t *answer = add_label(card, ascii(entry->assistant), font_bubble_text, COLOR_MUTED);
    lv_obj_set_width(answer, lv_pct(100));
    lv_obj_set_height(answer, ANSWER_LINES * lv_font_get_line_height(font_bubble_text) + TEXT_LINE_SPACE);
    lv_obj_set_style_text_line_space(answer, TEXT_LINE_SPACE, LV_PART_MAIN);
    lv_label_set_long_mode(answer, LV_LABEL_LONG_MODE_DOTS);

    lv_obj_t *del = lv_button_create(card);
    lv_obj_add_flag(del, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(del, DELETE_SIZE, DELETE_SIZE);
    lv_obj_align(del, LV_ALIGN_RIGHT_MID, DELETE_X, 0);
    lv_obj_set_style_radius(del, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(del, COLOR_DANGER_HALO, LV_PART_MAIN);
    lv_obj_set_style_bg_color(del, COLOR_DANGER, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(del, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(del, delete_clicked_cb, LV_EVENT_CLICKED, user_data);
    lv_obj_set_name(del, "history_delete_btn");
    lv_obj_t *icon = lv_image_create(del);
    lv_image_set_src(icon, img_icon_close_small);
    lv_obj_set_style_image_recolor(icon, COLOR_DANGER, LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_center(icon);
}

static void add_empty_state(void)
{
    lv_obj_t *avatar = lv_image_create(s_hist.list);
    lv_image_set_src(avatar, img_budgie_avatar);
    lv_obj_add_flag(avatar, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(avatar, LV_ALIGN_CENTER, 0, EMPTY_AVATAR_Y - LIST_Y / 2);
    lv_obj_t *title = add_label(s_hist.list, "No conversations yet", font_bubble_title, COLOR_TEXT);
    lv_obj_add_flag(title, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, EMPTY_TITLE_Y - LIST_Y / 2);
    lv_obj_set_name(title, "history_empty");
    lv_obj_t *hint = add_label(s_hist.list, "Ask me something with the mic or keyboard.", font_bubble_text,
                               COLOR_MUTED);
    lv_obj_add_flag(hint, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, EMPTY_HINT_Y - LIST_Y / 2);
}

static void rebuild(void)
{
    if (s_hist.screen == NULL) {
        return;
    }
    lv_obj_clean(s_hist.list);
    release_entries();
    const app_ui_callbacks_t *cb = app_ui_internal_callbacks();
    if (cb->list_history != NULL) {
        s_hist.count = cb->list_history(s_hist.entries, HISTORY_MAX, cb->user_ctx);
    }
    for (size_t i = 0; i < s_hist.count; i++) {
        add_card(i);
    }
    if (s_hist.count == 0) {
        add_empty_state();
        lv_obj_add_state(s_hist.clear_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(s_hist.clear_btn, LV_STATE_DISABLED);
    }
}

/* ---------- Clear all ---------- */

/* button -> row -> card -> backdrop; deleted after this event finishes */
static void dialog_close_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_obj_get_parent(lv_event_get_current_target_obj(e));
    lv_obj_delete_async(lv_obj_get_parent(lv_obj_get_parent(row)));
}

static void dialog_clear_cb(lv_event_t *e)
{
    const app_ui_callbacks_t *cb = app_ui_internal_callbacks();
    if (cb->on_clear_history != NULL) {
        cb->on_clear_history(cb->user_ctx);
    }
    dialog_close_cb(e);
    rebuild();
}

static void dialog_button(lv_obj_t *row, const char *text, const char *name, bool danger, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(row);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, DIALOG_BTN_HEIGHT);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, danger ? COLOR_DANGER : COLOR_KEY_FN, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, danger ? COLOR_DANGER : COLOR_KEY_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_name(btn, name);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(add_label(btn, text, font_bubble_title, danger ? lv_color_white() : COLOR_TEXT));
}

/* Dimmed backdrop + card: "Clear all history?" with Cancel / Clear all */
static void clear_all_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *dim = lv_obj_create(s_hist.screen);
    lv_obj_remove_style_all(dim);
    lv_obj_set_size(dim, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(dim, COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dim, LV_OPA_40, LV_PART_MAIN);
    lv_obj_add_flag(dim, LV_OBJ_FLAG_CLICKABLE);   /* blocks the list below */
    lv_obj_set_name(dim, "history_confirm");

    lv_obj_t *card = lv_obj_create(dim);
    lv_obj_set_size(card, DIALOG_WIDTH, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, DIALOG_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, COLOR_BUBBLE_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, COLOR_BUBBLE_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, DIALOG_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, DIALOG_ROW_GAP, LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    add_label(card, "Clear all history?", font_bubble_title, COLOR_TEXT);
    char msg[DIALOG_MSG_LEN];
    snprintf(msg, sizeof(msg), "Budgie will forget %u conversation%s.", (unsigned)s_hist.count,
             s_hist.count == 1 ? "" : "s");
    lv_obj_t *body = add_label(card, msg, font_bubble_text, COLOR_MUTED);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t *row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(row, DIALOG_BTN_TOP, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, DIALOG_BTN_GAP, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    dialog_button(row, "Cancel", "history_cancel_btn", false, dialog_close_cb);
    dialog_button(row, "Clear all", "history_clear_confirm_btn", true, dialog_clear_cb);
}

/* ---------- Screen ---------- */

static void screen_unloaded_cb(lv_event_t *e)
{
    lv_obj_delete_async(lv_event_get_current_target_obj(e));
}

static void screen_deleted_cb(lv_event_t *e)
{
    (void)e;
    release_entries();
    free(s_hist.text_buf);
    s_hist.text_buf = NULL;
    s_hist.screen = NULL;
    s_hist.list = NULL;
    s_hist.clear_btn = NULL;
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load_anim(home, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, SCREEN_ANIM_MS, 0, false);
}

static void build_header(void)
{
    lv_obj_t *back = lv_button_create(s_hist.screen);
    lv_obj_set_size(back, HEADER_BTN_SIZE, HEADER_BTN_SIZE);
    lv_obj_set_pos(back, HEADER_BTN_X, HEADER_BTN_Y);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(back, COLOR_KEY_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
    lv_obj_set_name(back, "history_back_btn");
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *icon = lv_image_create(back);
    lv_image_set_src(icon, img_icon_back);
    lv_obj_center(icon);

    lv_obj_t *title = add_label(s_hist.screen, "History", font_status, COLOR_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, TITLE_Y);

    lv_obj_t *clear = lv_button_create(s_hist.screen);
    lv_obj_set_size(clear, LV_SIZE_CONTENT, CLEAR_BTN_HEIGHT);
    lv_obj_align(clear, LV_ALIGN_TOP_RIGHT, CLEAR_BTN_X, CLEAR_BTN_Y);
    lv_obj_set_style_radius(clear, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(clear, COLOR_DANGER_HALO, LV_PART_MAIN);
    lv_obj_set_style_bg_color(clear, COLOR_DANGER_HALO, LV_STATE_DISABLED);
    lv_obj_set_style_opa(clear, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_set_style_shadow_width(clear, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(clear, CLEAR_BTN_PAD_HOR, LV_PART_MAIN);
    lv_obj_set_name(clear, "history_clear_btn");
    lv_obj_add_event_cb(clear, clear_all_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(add_label(clear, "Clear all", font_body, COLOR_DANGER));
    s_hist.clear_btn = clear;
}

static void open_history(void)
{
    if (s_hist.screen != NULL) {
        return;
    }
    s_hist.text_buf = heap_caps_malloc(TEXT_MAX, MALLOC_CAP_SPIRAM);
    lv_obj_t *screen = s_hist.text_buf != NULL ? lv_obj_create(NULL) : NULL;
    if (screen == NULL) {
        free(s_hist.text_buf);
        s_hist.text_buf = NULL;
        app_ui_internal_toast("Not enough memory for the history");
        return;
    }
    s_hist.screen = screen;
    lv_obj_set_name(screen, "history");
    lv_obj_set_style_bg_color(screen, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, screen_unloaded_cb, LV_EVENT_SCREEN_UNLOADED, NULL);
    lv_obj_add_event_cb(screen, screen_deleted_cb, LV_EVENT_DELETE, NULL);

    build_header();
    s_hist.list = lv_obj_create(screen);
    lv_obj_remove_style_all(s_hist.list);
    lv_obj_set_pos(s_hist.list, LIST_X, LIST_Y);
    lv_obj_set_size(s_hist.list, LIST_W, LV_VER_RES - LIST_Y);
    lv_obj_set_style_pad_row(s_hist.list, LIST_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(s_hist.list, LIST_GAP, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_hist.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_hist.list, LV_DIR_VER);
    lv_obj_set_name(s_hist.list, "history_list");
    rebuild();

    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, SCREEN_ANIM_MS, 0, false);
}

static void idle_bubble_clicked_cb(lv_event_t *e)
{
    (void)e;
    open_history();
}

void ui_history_init(void)
{
    lv_obj_t *bubble = home != NULL ? lv_obj_find_by_name(home, IDLE_BUBBLE) : NULL;
    if (bubble == NULL) {
        return;   /* export changed: no way to open the history */
    }
    /* The bubble's body and labels are not clickable, so taps land on the bubble root */
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bubble, idle_bubble_clicked_cb, LV_EVENT_CLICKED, NULL);
}
