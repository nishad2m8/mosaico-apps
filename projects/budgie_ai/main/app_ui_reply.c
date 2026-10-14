// SPDX-License-Identifier: Apache-2.0
//
// Reply bubble on the home screen: a long reply scrolls through once, and
// tapping the bubble stops the scroll and opens a popup with the full text.
// The same popup shows a full exchange from the History screen.
// Single owner: everything here runs on the LVGL task (see app_ui.c).

#include "app_ui_internal.h"

#include "budgie_ui.h"

#define REPLY_BUBBLE           "bubble_talking"
#define REPLY_TEXT_WIDTH       260
#define REPLY_SCROLL_DELAY_MS  1500    /* show the start of a long reply before it scrolls */
#define POPUP_WIDTH            420
#define POPUP_MAX_HEIGHT       400
#define POPUP_RADIUS           24
#define POPUP_PAD              24
#define POPUP_ROW_GAP          12
#define POPUP_TEXT_LINE_SPACE  4
#define POPUP_SHADOW_WIDTH     30
#define POPUP_CLOSE_SIZE       36
#define POPUP_CLOSE_X          6       /* close button offset into the card's top-right padding */
#define POPUP_CLOSE_Y          (-8)
#define POPUP_TITLE_PAD_RIGHT  (POPUP_CLOSE_SIZE - POPUP_CLOSE_X + 8)   /* keep wrapped titles clear of the close button */

static struct {
    lv_obj_t *bubble;
    lv_obj_t *title;
    lv_obj_t *text;
    lv_obj_t *popup;       /* dimmed backdrop on its screen, NULL while closed */
    lv_anim_t scroll_once; /* referenced by the text label's style */
} s_reply;

static void start_scroll(void)
{
    /* LVGL only scrolls text wider than the label, and takes the repeat count
     * and start delay from this template: one pass, ending back at the start */
    lv_label_set_long_mode(s_reply.text, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
}

static void stop_scroll(void)
{
    /* Dropping the scroll mode deletes the animation; the bubble shows the start with "..." */
    lv_label_set_long_mode(s_reply.text, LV_LABEL_LONG_MODE_DOTS);
}

static void popup_close(void)
{
    if (s_reply.popup != NULL) {
        lv_obj_delete(s_reply.popup);   /* popup_deleted_cb clears s_reply.popup */
    }
}

/* Also runs when the popup's screen is deleted with it open (History screen) */
static void popup_deleted_cb(lv_event_t *e)
{
    (void)e;
    s_reply.popup = NULL;
}

static void close_cb(lv_event_t *e)
{
    (void)e;
    popup_close();
}

static lv_obj_t *popup_label(lv_obj_t *parent, const char *text, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, COLOR_TEXT, LV_PART_MAIN);
    return label;
}

/* Dimmed backdrop (tap to close) holding a card with the full text and a close button */
void ui_reply_show_popup(lv_obj_t *screen, const char *title, const char *text)
{
    popup_close();
    lv_obj_t *dim = screen != NULL ? lv_obj_create(screen) : NULL;
    if (dim == NULL) {
        return;
    }
    s_reply.popup = dim;
    lv_obj_add_event_cb(dim, popup_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_name(dim, "reply_popup");
    lv_obj_remove_style_all(dim);
    lv_obj_set_size(dim, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(dim, COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dim, LV_OPA_40, LV_PART_MAIN);
    lv_obj_add_flag(dim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dim, close_cb, LV_EVENT_CLICKED, NULL);

    /* The card is clickable, so taps on it do not reach the backdrop; it scrolls long replies */
    lv_obj_t *card = lv_obj_create(dim);
    if (card == NULL) {
        popup_close();
        return;
    }
    lv_obj_set_width(card, POPUP_WIDTH);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(card, POPUP_MAX_HEIGHT, LV_PART_MAIN);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, POPUP_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, COLOR_BUBBLE_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, COLOR_BUBBLE_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, POPUP_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, POPUP_ROW_GAP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card, POPUP_SHADOW_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title_label = popup_label(card, title, font_bubble_title);
    lv_obj_set_width(title_label, lv_pct(100));
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_pad_right(title_label, POPUP_TITLE_PAD_RIGHT, LV_PART_MAIN);
    lv_obj_t *text_label = popup_label(card, text, font_bubble_text);
    lv_obj_set_width(text_label, lv_pct(100));
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(text_label, POPUP_TEXT_LINE_SPACE, LV_PART_MAIN);

    lv_obj_t *close = lv_button_create(card);
    if (close == NULL) {
        return;   /* the backdrop still closes the popup */
    }
    lv_obj_add_flag(close, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(close, POPUP_CLOSE_SIZE, POPUP_CLOSE_SIZE);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, POPUP_CLOSE_X, POPUP_CLOSE_Y);
    lv_obj_add_style(close, &style_round_btn, LV_PART_MAIN);
    lv_obj_add_style(close, &style_round_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_name(close, "reply_close_btn");
    lv_obj_add_event_cb(close, close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *icon = lv_image_create(close);
    lv_image_set_src(icon, img_icon_close_small);
    lv_obj_center(icon);
}

static void bubble_clicked_cb(lv_event_t *e)
{
    (void)e;
    stop_scroll();
    ui_reply_show_popup(home, lv_label_get_text(s_reply.title), lv_label_get_text(s_reply.text));
}

void ui_reply_init(void)
{
    s_reply.bubble = home != NULL ? lv_obj_find_by_name(home, REPLY_BUBBLE) : NULL;
    s_reply.title = s_reply.bubble != NULL ? lv_obj_find_by_name(s_reply.bubble, "title_label") : NULL;
    s_reply.text = s_reply.bubble != NULL ? lv_obj_find_by_name(s_reply.bubble, "text_label") : NULL;
    if (s_reply.title == NULL || s_reply.text == NULL) {
        s_reply.bubble = NULL;   /* export changed: leave the bubble as exported */
        return;
    }

    /* One line, so a long reply cannot push the bubble into the buttons */
    lv_obj_set_width(s_reply.text, REPLY_TEXT_WIDTH);
    lv_obj_set_height(s_reply.text,
                      lv_font_get_line_height(lv_obj_get_style_text_font(s_reply.text, LV_PART_MAIN)));
    lv_obj_set_style_text_align(s_reply.text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_anim_init(&s_reply.scroll_once);
    lv_anim_set_repeat_count(&s_reply.scroll_once, 1);
    lv_anim_set_delay(&s_reply.scroll_once, REPLY_SCROLL_DELAY_MS);
    lv_obj_set_style_anim(s_reply.text, &s_reply.scroll_once, LV_PART_MAIN);
    start_scroll();

    /* The bubble's body and labels are not clickable, so taps land on the bubble root */
    lv_obj_add_flag(s_reply.bubble, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_reply.bubble, bubble_clicked_cb, LV_EVENT_CLICKED, NULL);
}

void ui_reply_set(const char *title, const char *text)
{
    if (s_reply.bubble == NULL) {
        return;
    }
    popup_close();
    if (title != NULL) {
        lv_label_set_text(s_reply.title, title);
    }
    start_scroll();
    if (text != NULL) {
        lv_label_set_text(s_reply.text, text);
    }
}

bool ui_reply_popup_open(void)
{
    return s_reply.popup != NULL;
}
