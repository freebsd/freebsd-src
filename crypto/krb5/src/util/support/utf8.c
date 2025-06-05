/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/utf8.c */
/*
 * Copyright 2008 by the Massachusetts Institute of Technology.
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

/* This work is part of OpenLDAP Software <https://www.openldap.org/>. */

/* Basic UTF-8 routines
 *
 * These routines are "dumb".  Though they understand UTF-8,
 * they don't grok Unicode.  That is, they can push bits,
 * but don't have a clue what the bits represent.  That's
 * good enough for use with the KRB5 Client SDK.
 *
 * These routines are not optimized.
 */

#include "k5-platform.h"
#include "k5-utf8.h"
#include "supp-int.h"

/*
 * Returns length indicated by first byte.
 */
const char krb5int_utf8_lentab[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/*
 * Make sure the UTF-8 char used the shortest possible encoding
 * returns charlen if valid, 0 if not.
 *
 * Here are the valid UTF-8 encodings, taken from RFC 3629 page 4.
 * The table is slightly modified from that of the RFC.
 *
 * UCS-4 range (hex)      UTF-8 sequence (binary)
 * 0000 0000-0000 007F   0.......
 * 0000 0080-0000 07FF   110++++. 10......
 * 0000 0800-0000 FFFF   1110++++ 10+..... 10......
 * 0001 0000-0010 FFFF   11110+++ 10++.... 10...... 10......
 *
 * The '.' bits are "don't cares". When validating a UTF-8 sequence,
 * at least one of the '+' bits must be set, otherwise the character
 * should have been encoded in fewer octets. Note that in the two-octet
 * case, only the first octet needs to be validated, and this is done
 * in the krb5int_utf8_lentab[] above.
 */

/* mask of required bits in second octet */
#undef c
#define c const char
c krb5int_utf8_mintab[] = {
    (c)0x20, (c)0x80, (c)0x80, (c)0x80, (c)0x80, (c)0x80, (c)0x80, (c)0x80,
    (c)0x80, (c)0x80, (c)0x80, (c)0x80, (c)0x80, (c)0x80, (c)0x80, (c)0x80,
    (c)0x30, (c)0x80, (c)0x80, (c)0x80, (c)0x80, (c)0x00, (c)0x00, (c)0x00,
    (c)0x00, (c)0x00, (c)0x00, (c)0x00, (c)0x00, (c)0x00, (c)0x00, (c)0x00 };
#undef c

/*
 * Convert a UTF8 character to a UCS4 character.  Return 0 on success,
 * -1 on failure.
 */
int krb5int_utf8_to_ucs4(const char *p, krb5_ucs4 *out)
{
    const unsigned char *c = (const unsigned char *) p;
    krb5_ucs4 ch;
    int len, i;
    static unsigned char mask[] = {
        0, 0x7f, 0x1f, 0x0f, 0x07 };

    *out = 0;
    len = KRB5_UTF8_CHARLEN2(p, len);

    if (len == 0)
        return -1;

    ch = c[0] & mask[len];

    for (i = 1; i < len; i++) {
        if ((c[i] & 0xc0) != 0x80)
            return -1;

        ch <<= 6;
        ch |= c[i] & 0x3f;
    }

    if (ch > 0x10ffff)
        return -1;

    *out = ch;
    return 0;
}

/* conv UCS-4 to UTF-8 */
size_t krb5int_ucs4_to_utf8(krb5_ucs4 c, char *buf)
{
    size_t len = 0;
    unsigned char *p = (unsigned char *) buf;

    /* not a valid Unicode character */
    if (c > 0x10ffff)
        return 0;

    /* Just return length, don't convert */
    if (buf == NULL) {
        if (c < 0x80) return 1;
        else if (c < 0x800) return 2;
        else if (c < 0x10000) return 3;
        else return 4;
    }

    if (c < 0x80) {
        p[len++] = c;
    } else if (c < 0x800) {
        p[len++] = 0xc0 | ( c >> 6 );
        p[len++] = 0x80 | ( c & 0x3f );
    } else if (c < 0x10000) {
        p[len++] = 0xe0 | ( c >> 12 );
        p[len++] = 0x80 | ( (c >> 6) & 0x3f );
        p[len++] = 0x80 | ( c & 0x3f );
    } else /* if (c < 0x110000) */ {
        p[len++] = 0xf0 | ( c >> 18 );
        p[len++] = 0x80 | ( (c >> 12) & 0x3f );
        p[len++] = 0x80 | ( (c >> 6) & 0x3f );
        p[len++] = 0x80 | ( c & 0x3f );
    }

    return len;
}
