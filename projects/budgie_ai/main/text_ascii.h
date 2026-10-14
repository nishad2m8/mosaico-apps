// SPDX-License-Identifier: Apache-2.0
//
// The UI fonts cover printable ASCII only (0x20-0x7E, see globals.xml).
// Convert model output to that range: common Unicode punctuation becomes its
// ASCII look-alike, markdown emphasis is dropped, anything else is removed.
#pragma once

#include <stddef.h>

/** Write the ASCII version of UTF-8 @p in into @p out (always NUL-terminated). */
void text_to_ascii(const char *in, char *out, size_t out_size);
