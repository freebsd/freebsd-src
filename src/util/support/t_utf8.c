/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/t_utf8.c - test UTF-8 boundary conditions */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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

#include <stdio.h>
#include <string.h>

#include "k5-platform.h"
#include "k5-utf8.h"

/*
 * Convenience macro to allow testing of old encodings.
 *
 * "Old" means ISO/IEC 10646 prior to 2011, when the highest valid code point
 * was U+7FFFFFFF instead of U+10FFFF.
 */
#ifdef OLDENCODINGS
#define L(x) (x)
#else
#define L(x) 0
#endif

/*
 * len is 0 for invalid encoding prefixes (krb5int_utf8_charlen2() partially
 * enforces the validity of the first two bytes, based on masking the second
 * byte.  It doesn't check whether bit 6 is 0, though, and doesn't catch the
 * range between U+110000 and U+13FFFF).
 *
 * ucs is 0 for invalid encodings (including ones with valid prefixes according
 * to krb5int_utf8_charlen2(); krb5int_utf8_to_ucs4() will still fail on them
 * because it checks more things.)  Code points above U+10FFFF are excluded by
 * the actual test code and remain in the table for possibly testing the old
 * implementation that didn't exclude them.
 *
 * Neither krb5int_ucs4_to_utf8() nor krb5int_utf8_to_ucs4() excludes the
 * surrogate pair range.
 */
struct testcase {
    const char *p;
    krb5_ucs4 ucs;
    int len;
} testcases[] = {
    { "\x7f", 0x0000007f, 1 },             /* Lowest 1-byte encoding */
    { "\xc0\x80", 0x00000000, 0 },         /* Invalid 2-byte encoding */
    { "\xc2\x80", 0x00000080, 2 },         /* Lowest valid 2-byte encoding */
    { "\xdf\xbf", 0x000007ff, 2 },         /* Highest valid 2-byte encoding*/
    { "\xdf\xff", 0x00000000, 2 },         /* Invalid 2-byte encoding*/
    { "\xe0\x80\x80", 0x00000000, 0 },     /* Invalid 3-byte encoding */
    { "\xe0\xa0\x80", 0x00000800, 3 },     /* Lowest valid 3-byte encoding */
    { "\xef\xbf\xbf", 0x0000ffff, 3 },     /* Highest valid 3-byte encoding */
    { "\xef\xff\xff", 0x00000000, 3 },     /* Invalid 3-byte encoding */
    { "\xf0\x80\x80\x80", 0x00000000, 0 }, /* Invalid 4-byte encoding */
    { "\xf0\x90\x80\x80", 0x00010000, 4 }, /* Lowest valid 4-byte encoding */
    { "\xf4\x8f\xbf\xbf", 0x0010ffff, 4 }, /* Highest valid 4-byte encoding */
    /* Next higher 4-byte encoding (old) */
    { "\xf4\x90\x80\x80", 0x00110000, 4 },
    /* Highest 4-byte encoding starting with 0xf4 (old) */
    { "\xf4\xbf\xbf\xbf", 0x0013ffff, 4 },
    /* Next higher 4-byte prefix byte (old) */
    { "\xf5\x80\x80\x80", 0x00140000, L(4) },
    /* Highest valid 4-byte encoding (old) */
    { "\xf7\xbf\xbf\xbf", 0x001fffff, L(4) },
    /* Invalid 4-byte encoding */
    { "\xf7\xff\xff\xff", 0x00000000, L(4) },
    /* Invalid 5-byte encoding */
    { "\xf8\x80\x80\x80\x80", 0x00000000, 0 },
    /* Lowest valid 5-byte encoding (old) */
    { "\xf8\x88\x80\x80\x80", 0x00200000, L(5) },
    /* Highest valid 5-byte encoding (old) */
    { "\xfb\xbf\xbf\xbf\xbf", 0x03ffffff, L(5) },
    /* Invalid 5-byte encoding */
    { "\xfb\xff\xff\xff\xff", 0x00000000, L(5) },
    /* Invalid 6-byte encoding */
    { "\xfc\x80\x80\x80\x80\x80", 0x00000000, 0 },
    /* Lowest valid 6-byte encoding (old) */
    { "\xfc\x84\x80\x80\x80\x80", 0x04000000, L(6) },
    /* Highest valid 6-byte encoding (old) */
    { "\xfd\xbf\xbf\xbf\xbf\xbf", 0x7fffffff, L(6) },
    /* Invalid 6-byte encoding */
    { "\xfd\xff\xff\xff\xff\xff", 0x00000000, L(6) },
};

static void
printhex(const char *p)
{
    for (; *p != '\0'; p++) {
        printf("%02x ", (unsigned char)*p);
    }
}

static void
printtest(struct testcase *t)
{
    printhex(t->p);
    printf("0x%08lx, %d\n", (unsigned long)t->ucs, t->len);
}

static int
test_decode(struct testcase *t, int high4)
{
    int len, status = 0;
    krb5_ucs4 u = 0;

    len = krb5int_utf8_charlen2(t->p);
    if (len != t->len) {
        printf("expected len=%d, got len=%d\n", t->len, len);
        status = 1;
    }
    if ((t->len == 0 || high4) && krb5int_utf8_to_ucs4(t->p, &u) != -1) {
        printf("unexpected success in utf8_to_ucs4\n");
        status = 1;
    }
    if (krb5int_utf8_to_ucs4(t->p, &u) != 0 && t->ucs != 0 && !high4) {
        printf("unexpected failure in utf8_to_ucs4\n");
        status = 1;
    }
    if (t->ucs != u && !high4) {
        printf("expected 0x%08lx, got 0x%08lx\n", (unsigned long)t->ucs,
               (unsigned long)u);
        status = 1;
    }
    return status;
}

static int
test_encode(struct testcase *t, int high4)
{
    size_t size;
    char buf[7];

    memset(buf, 0, sizeof(buf));
    size = krb5int_ucs4_to_utf8(t->ucs, buf);
    if (high4 && size != 0) {
        printf("unexpected success beyond U+10FFFF\n");
        return 1;
    }
    if (!high4 && size == 0) {
        printf("unexpected zero size on encode\n");
        return 1;
    }
    if (size != 0 && strcmp(t->p, buf) != 0) {
        printf("expected ");
        printhex(t->p);
        printf("got ");
        printhex(buf);
        printf("\n");
        return 1;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    size_t ncases = sizeof(testcases) / sizeof(testcases[0]);
    size_t i;
    struct testcase *t;
    int status = 0, verbose = 0;
    /* Is this a "high" 4-byte encoding above U+10FFFF? */
    int high4;

    if (argc == 2 && strcmp(argv[1], "-v") == 0)
        verbose = 1;
    for (i = 0; i < ncases; i++) {
        t = &testcases[i];
        if (verbose)
            printtest(t);
#ifndef OLDENCODINGS
        high4 = t->ucs > 0x10ffff;
#else
        high4 = 0;
#endif
        if (test_decode(t, high4) != 0)
            status = 1;
        if (t->ucs == 0)
            continue;
        if (test_encode(t, high4) != 0)
            status = 1;
    }
    return status;
}
