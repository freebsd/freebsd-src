/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/t_utf16.c - test UTF-16 conversion functions */
/*
 * Copyright (C) 2017 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This program tests conversions between UTF-8 and little-endian UTF-16, with
 * an eye mainly towards covering UTF-16 edge cases and UTF-8 decoding results
 * which we detect as invalid in utf8_conv.c.  t_utf8.c covers more UTF-8 edge
 * cases.
 */

#include <stdio.h>
#include <string.h>

#include "k5-platform.h"
#include "k5-utf8.h"

struct test {
    const char *utf8;
    const char *utf16;
    size_t utf16len;
} tests[] = {
    { "", "", 0 },
    { "abcd", "a\0b\0c\0d\0", 8 },
    /* From RFC 2781 (tests code point 0x12345 and some ASCII) */
    { "\xF0\x92\x8D\x85=Ra", "\x08\xD8\x45\xDF=\0R\0a\0", 10 },
    /* Lowest and highest Supplementary Plane code points */
    { "\xF0\x90\x80\x80 \xF4\x8F\xBF\xBF",
      "\x00\xD8\x00\xDC \0\xFF\xDB\xFF\xDF", 10 },
    /* Basic Multilingual Plane code points near and above surrogate range */
    { "\xED\x9F\xBF", "\xFF\xD7", 2 },
    { "\xEE\x80\x80 \xEE\xBF\xBF", "\x00\xE0 \0\xFF\xEF", 6 },
    /* Invalid UTF-8: decodes to value in surrogate pair range */
    { "\xED\xA0\x80", NULL, 0 }, /* 0xD800 */
    { "\xED\xAF\xBF", NULL, 0 }, /* 0xDBFF */
    { "\xED\xB0\x80", NULL, 0 }, /* 0xDC00 */
    { "\xED\xBF\xBF", NULL, 0 }, /* 0xDFFF */
    /* Invalid UTF-8: decodes to value above Unicode range */
    { "\xF4\x90\x80\x80", NULL, 0 },
    { "\xF4\xBF\xBF\xBF", NULL, 0 },
    { "\xF5\x80\x80\x80", NULL, 0 }, /* thrown out early due to first byte */
    /* Invalid UTF-16: odd numbers of UTF-16 bytes */
    { NULL, "\x00", 1 },
    { NULL, "\x01\x00\x02", 3 },
    /* Invalid UTF-16: high surrogate without a following low surrogate */
    { NULL, "\x00\xD8\x00\x00", 4 },
    { NULL, "\x00\xD8\xFF\xDB", 4 },
    { NULL, "\xFF\xDB", 2 },
    /* Invalid UTF-16: low surrogate without a preceding high surrogate */
    { NULL, "\x61\x00\x00\xDC", 4 },
    { NULL, "\xFF\xDF\xFF\xDB", 4 },
};

int
main(int argc, char **argv)
{
    int ret;
    struct test *t;
    size_t i, utf16len;
    uint8_t *utf16;
    char *utf8;

    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        t = &tests[i];
        if (t->utf8 != NULL) {
            ret = k5_utf8_to_utf16le(t->utf8, &utf16, &utf16len);
            if (t->utf16 == NULL) {
                assert(ret == EINVAL);
            } else {
                assert(ret == 0);
                assert(t->utf16len == utf16len);
                assert(memcmp(t->utf16, utf16, utf16len) == 0);
                free(utf16);
            }
        }

        if (t->utf16 != NULL) {
            ret = k5_utf16le_to_utf8((uint8_t *)t->utf16, t->utf16len, &utf8);
            if (t->utf8 == NULL) {
                assert(ret == EINVAL);
            } else {
                assert(ret == 0);
                assert(strcmp(t->utf8, utf8) == 0);
                free(utf8);
            }
        }
    }
    return 0;
}
