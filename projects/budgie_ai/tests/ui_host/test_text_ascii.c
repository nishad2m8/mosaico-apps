#include <stdio.h>
#include <string.h>
#include "text_ascii.h"
int main(void) {
    char out[64]; int f = 0;
    struct { const char *in, *want; } t[] = {
        { "It\xe2\x80\x99s **8,849 m** \xe2\x80\x94 wow\xe2\x80\xa6", "It's 8,849 m  -  wow..." },
        { "Caf\xc3\xa9 \xf0\x9f\x98\x80 ok", "Cafe  ok" },
        { "# Title\n`code`", " Title\ncode" },
        { "\xe2\x80\x9cquote\xe2\x80\x9d", "\"quote\"" },
    };
    for (unsigned i = 0; i < sizeof(t)/sizeof(t[0]); i++) {
        text_to_ascii(t[i].in, out, sizeof(out));
        int ok = strcmp(out, t[i].want) == 0; f += !ok;
        printf("%s [%s]\n", ok ? "PASS" : "FAIL", out);
    }
    text_to_ascii("abcdefghij", out, 5); f += strcmp(out, "abcd") != 0;
    printf("%s truncation [%s]\n", strcmp(out, "abcd") ? "FAIL" : "PASS", out);
    return f;
}
