// SPDX-License-Identifier: Apache-2.0

#include "text_ascii.h"

#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t codepoint;
    const char *ascii;
} replacement_t;

static const replacement_t s_replacements[] = {
    { 0x00A0, " " },  { 0x2018, "'" },  { 0x2019, "'" },  { 0x201A, "'" },
    { 0x201C, "\"" }, { 0x201D, "\"" }, { 0x201E, "\"" }, { 0x2013, "-" },
    { 0x2014, " - " }, { 0x2212, "-" }, { 0x2026, "..." }, { 0x2022, "-" },
    { 0x00B7, "-" },  { 0x00D7, "x" },  { 0x00B0, " deg" }, { 0x20AC, "EUR" },
    { 0x00A3, "GBP" }, { 0x00E9, "e" }, { 0x00E8, "e" },  { 0x00E0, "a" },
    { 0x00E7, "c" },  { 0x00F1, "n" },  { 0x00FC, "u" },  { 0x00F6, "o" },
    { 0x00E4, "a" },
};

/* Decode one UTF-8 sequence; returns its length (>= 1) */
static size_t decode(const unsigned char *s, uint32_t *cp)
{
    if (s[0] < 0x80) {
        *cp = s[0];
        return 1;
    }
    size_t len = (s[0] & 0xE0) == 0xC0 ? 2 : (s[0] & 0xF0) == 0xE0 ? 3 : (s[0] & 0xF8) == 0xF0 ? 4 : 1;
    uint32_t v = len == 2 ? (s[0] & 0x1F) : len == 3 ? (s[0] & 0x0F) : (s[0] & 0x07);
    for (size_t i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *cp = 0xFFFD;
            return i;   /* truncated sequence */
        }
        v = (v << 6) | (s[i] & 0x3F);
    }
    *cp = len == 1 ? 0xFFFD : v;
    return len;
}

void text_to_ascii(const char *in, char *out, size_t out_size)
{
    if (out_size == 0) {
        return;
    }
    size_t o = 0;
    const unsigned char *p = (const unsigned char *)(in != NULL ? in : "");
    while (*p != '\0' && o + 1 < out_size) {
        uint32_t cp;
        p += decode(p, &cp);
        const char *rep = NULL;
        char one[2] = { 0 };
        if (cp == '*' || cp == '#' || cp == '`') {
            continue;                       /* markdown emphasis / headings / code */
        } else if (cp == '\n' || (cp >= 0x20 && cp < 0x7F)) {
            one[0] = (char)cp;
            rep = one;
        } else if (cp == '\t' || cp == '\r') {
            rep = " ";
        } else {
            for (size_t i = 0; i < sizeof(s_replacements) / sizeof(s_replacements[0]); i++) {
                if (s_replacements[i].codepoint == cp) {
                    rep = s_replacements[i].ascii;
                    break;
                }
            }
        }
        for (; rep != NULL && *rep != '\0' && o + 1 < out_size; rep++) {
            out[o++] = *rep;
        }
    }
    out[o] = '\0';
}
