/**
 * @file budgie_chat.c
 * Chat screen logic: T9 multi-tap typing and the hooks to connect the assistant.
 *
 * Multi-tap: tapping the same key again within T9_MULTITAP_MS replaces the last letter with the
 * next one on that key ("2" -> a, b, c, 2). A different key, or a pause, starts a new letter.
 * The case key toggles abc/ABC, the mode key toggles letters/digits.
 * Send accepts text, a photo attached from the camera, or both.
 */

#include "budgie_chat.h"
#include "budgie_ui_gen.h"   /* permanent screen objects (chat) */
#include <string.h>
#include <stdio.h>

#define T9_MULTITAP_MS  900
#define CHAT_MAX_LEN    256
#define CHAT_GREETING   "Hi! Type your message on the keypad and tap Send. I'll answer here."

/* Set by Settings > Clear chat history; the (permanent) chat screen resets the next time it opens */
static bool history_clear_pending;

static struct {
    char key;           /* digit of the last letter key, 0 if the next tap starts a new letter */
    uint32_t tick;      /* time of the last letter key tap */
    uint32_t idx;       /* position in the key's character set */
    bool upper;
    bool numeric;
} t9;

static const char * t9_chars(char digit)
{
    switch(digit) {
        case '1': return ".,?!'1";
        case '2': return "abc2";
        case '3': return "def3";
        case '4': return "ghi4";
        case '5': return "jkl5";
        case '6': return "mno6";
        case '7': return "pqrs7";
        case '8': return "tuv8";
        case '9': return "wxyz9";
        default:  return "";
    }
}

/* Find a named widget on the screen the event came from */
static lv_obj_t * find_on_screen(lv_event_t * e, const char * name)
{
    lv_obj_t * screen = lv_obj_get_screen(lv_event_get_current_target_obj(e));
    return lv_obj_find_by_name(screen, name);
}

static void insert_char(lv_obj_t * ta, char c)
{
    char s[2] = {c, '\0'};
    lv_textarea_add_text(ta, s);
}

void chat_t9_key_cb(lv_event_t * e)
{
    const char * digit = lv_event_get_user_data(e);
    lv_obj_t * ta = find_on_screen(e, "chat_input");
    if(ta == NULL || digit == NULL || digit[0] == '\0') return;

    char d = digit[0];
    if(t9.numeric) {
        insert_char(ta, d);
        t9.key = 0;
        return;
    }

    const char * set = t9_chars(d);
    size_t n = strlen(set);
    if(n == 0) return;

    if(t9.key == d && lv_tick_elaps(t9.tick) < T9_MULTITAP_MS) {
        t9.idx = (t9.idx + 1) % n;
        lv_textarea_delete_char(ta);    /* replace the letter typed by the previous tap */
    }
    else {
        t9.idx = 0;
    }

    char c = set[t9.idx];
    if(t9.upper && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    insert_char(ta, c);

    t9.key = d;
    t9.tick = lv_tick_get();
}

void chat_backspace_cb(lv_event_t * e)
{
    lv_obj_t * ta = find_on_screen(e, "chat_input");
    if(ta) lv_textarea_delete_char(ta);
    t9.key = 0;
}

void chat_space_cb(lv_event_t * e)
{
    lv_obj_t * ta = find_on_screen(e, "chat_input");
    if(ta) insert_char(ta, ' ');
    t9.key = 0;
}

void chat_clear_cb(lv_event_t * e)
{
    lv_obj_t * ta = find_on_screen(e, "chat_input");
    if(ta) lv_textarea_set_text(ta, "");
    t9.key = 0;
}

void chat_case_cb(lv_event_t * e)
{
    t9.upper = !t9.upper;
    t9.key = 0;
    lv_obj_t * label = find_on_screen(e, "case_label");
    if(label) lv_label_set_text(label, t9.upper ? "ABC" : "abc");
}

void chat_mode_cb(lv_event_t * e)
{
    t9.numeric = !t9.numeric;
    t9.key = 0;
    lv_obj_t * label = find_on_screen(e, "mode_label");
    if(label) lv_label_set_text(label, t9.numeric ? "abc" : "123");   /* shows what the key switches to */
}

void chat_send_cb(lv_event_t * e)
{
    lv_obj_t * ta = find_on_screen(e, "chat_input");
    if(ta == NULL) return;

    /* The photo tile is shown (by subject_chat_has_photo) while a photo is attached */
    lv_obj_t * tile = find_on_screen(e, "photo_tile");
    bool with_photo = tile && !lv_obj_has_flag(tile, LV_OBJ_FLAG_HIDDEN);

    const char * typed = lv_textarea_get_text(ta);
    if(typed == NULL) typed = "";
    if(typed[0] == '\0' && !with_photo) return;

    char msg[CHAT_MAX_LEN];
    lv_snprintf(msg, sizeof(msg), "%s", typed);
    lv_textarea_set_text(ta, "");
    t9.key = 0;

    lv_obj_t * response = find_on_screen(e, "response");
    if(response) lv_label_set_text(response, "Hmm, let me think...");

    budgie_chat_on_send(msg, with_photo);
}

void chat_screen_load_cb(lv_event_t * e)
{
    if(!history_clear_pending) return;
    history_clear_pending = false;

    lv_obj_t * response = find_on_screen(e, "response");
    if(response) lv_label_set_text(response, CHAT_GREETING);
    lv_obj_t * ta = find_on_screen(e, "chat_input");
    if(ta) lv_textarea_set_text(ta, "");
    t9.key = 0;
}

void settings_clear_history_cb(lv_event_t * e)
{
    history_clear_pending = true;
    budgie_chat_on_history_cleared();

    /* Confirm on the button itself and disable it so it is not tapped twice */
    lv_obj_t * btn = lv_event_get_current_target_obj(e);
    lv_obj_t * label = lv_obj_find_by_name(btn, "clear_history_label");
    if(label) lv_label_set_text(label, "Cleared");
    lv_obj_add_state(btn, LV_STATE_DISABLED);
}

void budgie_chat_set_response(const char * text)
{
    /* `chat` is the permanent chat screen created by budgie_ui_init() */
    lv_obj_t * root = chat != NULL ? chat : lv_screen_active();
    lv_obj_t * response = lv_obj_find_by_name(root, "response");
    if(response) lv_label_set_text(response, text);
}

#ifndef BUDGIE_CHAT_EXTERNAL_SEND
/* Placeholder until the assistant is connected: echo the message back */
void budgie_chat_on_send(const char * text, bool with_photo)
{
    char reply[CHAT_MAX_LEN + 96];
    if(text[0] == '\0') {
        lv_snprintf(reply, sizeof(reply), "You sent a photo. Connect budgie_chat_on_send() to get real answers.");
    }
    else {
        lv_snprintf(reply, sizeof(reply), "You said: \"%s\"%s. Connect budgie_chat_on_send() to get real answers.",
                    text, with_photo ? " with a photo" : "");
    }
    budgie_chat_set_response(reply);
}

void budgie_chat_on_history_cleared(void)
{
}
#endif
