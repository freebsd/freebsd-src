/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/utf8_conv.c */
/*
 * Copyright 2008, 2017 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */
/*
 * Copyright 1998-2008 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <https://www.OpenLDAP.org/license.html>.
 */
/* Copyright (C) 1999, 2000 Novell, Inc. All Rights Reserved.
 *
 * THIS WORK IS SUBJECT TO U.S. AND INTERNATIONAL COPYRIGHT LAWS AND
 * TREATIES. USE, MODIFICATION, AND REDISTRIBUTION OF THIS WORK IS SUBJECT
 * TO VERSION 2.0.1 OF THE OPENLDAP PUBLIC LICENSE, A COPY OF WHICH IS
 * AVAILABLE AT HTTP://WWW.OPENLDAP.ORG/LICENSE.HTML OR IN THE FILE "LICENSE"
 * IN THE TOP-LEVEL DIRECTORY OF THE DISTRIBUTION. ANY USE OR EXPLOITATION
 * OF THIS WORK OTHER THAN AS AUTHORIZED IN VERSION 2.0.1 OF THE OPENLDAP
 * PUBLIC LICENSE, OR OTHER PRIOR WRITTEN CONSENT FROM NOVELL, COULD SUBJECT
 * THE PERPETRATOR TO CRIMINAL AND CIVIL LIABILITY.
 */

/* This work is based on OpenLDAP Software <https://www.openldap.org/>. */

/*
 * These routines convert between UTF-16 and UTF-8.  UTF-16 encodes a Unicode
 * character in either two or four bytes.  Characters in the Basic Multilingual
 * Plane (hex 0..D7FF and E000..FFFF) are encoded as-is in two bytes.
 * Characters in the Supplementary Planes (10000..10FFFF) are split into a high
 * surrogate and a low surrogate, each containing ten bits of the character
 * value, and encoded in four bytes.
 */

#include "k5-platform.h"
#include "k5-utf8.h"
#include "k5-buf.h"
#include "k5-input.h"
#include "supp-int.h"

static unsigned char mask[] = { 0, 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };

/* A high surrogate is ten bits masked with 0xD800. */
#define IS_HIGH_SURROGATE(c) ((c) >= 0xD800 && (c) <= 0xDBFF)

/* A low surrogate is ten bits masked with 0xDC00. */
#define IS_LOW_SURROGATE(c) ((c) >= 0xDC00 && (c) <= 0xDFFF)

/* A valid Unicode code point is in the range 0..10FFFF and is not a surrogate
 * value. */
#define IS_SURROGATE(c) ((c) >= 0xD800 && (c) <= 0xDFFF)
#define IS_VALID_UNICODE(c) ((c) <= 0x10FFFF && !IS_SURROGATE(c))

/* A Basic Multilingual Plane character is in the range 0..FFFF and is not a
 * surrogate value. */
#define IS_BMP(c) ((c) <= 0xFFFF && !IS_SURROGATE(c))

/* Characters in the Supplementary Planes have a base value subtracted from
 * their code points to form a 20-bit value; ten bits go in each surrogate. */
#define BASE 0x10000
#define HIGH_SURROGATE(c) (0xD800 | (((c) - BASE) >> 10))
#define LOW_SURROGATE(c) (0xDC00 | (((c) - BASE) & 0x3FF))
#define COMPOSE(c1, c2) (BASE + ((((c1) & 0x3FF) << 10) | ((c2) & 0x3FF)))

int
k5_utf8_to_utf16le(const char *utf8, uint8_t **utf16_out, size_t *nbytes_out)
{
    struct k5buf buf;
    krb5_ucs4 ch;
    size_t chlen, i;

    *utf16_out = NULL;
    *nbytes_out = 0;

    /* UTF-16 conversion is used for RC4 string-to-key, so treat this data as
     * sensitive. */
    k5_buf_init_dynamic_zap(&buf);

    /* Examine next UTF-8 character. */
    while (*utf8 != '\0') {
        /* Get UTF-8 sequence length from first byte. */
        chlen = KRB5_UTF8_CHARLEN2(utf8, chlen);
        if (chlen == 0)
            goto invalid;

        /* First byte minus length tag */
        ch = (krb5_ucs4)(utf8[0] & mask[chlen]);

        for (i = 1; i < chlen; i++) {
            /* Subsequent bytes must start with 10. */
            if ((utf8[i] & 0xc0) != 0x80)
                goto invalid;

            /* 6 bits of data in each subsequent byte */
            ch <<= 6;
            ch |= (krb5_ucs4)(utf8[i] & 0x3f);
        }
        if (!IS_VALID_UNICODE(ch))
            goto invalid;

        /* Characters in the basic multilingual plane are encoded using two
         * bytes; other characters are encoded using four bytes. */
        if (IS_BMP(ch)) {
            k5_buf_add_uint16_le(&buf, ch);
        } else {
            /* 0x10000 is subtracted from ch; then the high ten bits plus
             * 0xD800 and the low ten bits plus 0xDC00 are the surrogates. */
            k5_buf_add_uint16_le(&buf, HIGH_SURROGATE(ch));
            k5_buf_add_uint16_le(&buf, LOW_SURROGATE(ch));
        }

        /* Move to next UTF-8 character. */
        utf8 += chlen;
    }

    *utf16_out = buf.data;
    *nbytes_out = buf.len;
    return 0;

invalid:
    k5_buf_free(&buf);
    return EINVAL;
}

int
k5_utf16le_to_utf8(const uint8_t *utf16bytes, size_t nbytes, char **utf8_out)
{
    struct k5buf buf;
    struct k5input in;
    uint16_t ch1, ch2;
    krb5_ucs4 ch;
    size_t chlen;
    void *p;

    *utf8_out = NULL;

    if (nbytes % 2 != 0)
        return EINVAL;

    k5_buf_init_dynamic(&buf);
    k5_input_init(&in, utf16bytes, nbytes);
    while (!in.status && in.len > 0) {
        /* Get the next character or high surrogate.  A low surrogate without a
         * preceding high surrogate is invalid. */
        ch1 = k5_input_get_uint16_le(&in);
        if (IS_LOW_SURROGATE(ch1))
            goto invalid;
        if (IS_HIGH_SURROGATE(ch1)) {
            /* Get the low surrogate and combine the pair. */
            ch2 = k5_input_get_uint16_le(&in);
            if (!IS_LOW_SURROGATE(ch2))
                goto invalid;
            ch = COMPOSE(ch1, ch2);
        } else {
            ch = ch1;
        }

        chlen = krb5int_ucs4_to_utf8(ch, NULL);
        p = k5_buf_get_space(&buf, chlen);
        if (p == NULL)
            return ENOMEM;
        (void)krb5int_ucs4_to_utf8(ch, p);
    }

    if (in.status)
        goto invalid;

    *utf8_out = k5_buf_cstring(&buf);
    return (*utf8_out == NULL) ? ENOMEM : 0;

invalid:
    k5_buf_free(&buf);
    return EINVAL;
}
