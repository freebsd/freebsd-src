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
 * <http://www.OpenLDAP.org/license.html>.
 */

/* This work is part of OpenLDAP Software <http://www.openldap.org/>. */

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
 * return the number of bytes required to hold the
 * NULL-terminated UTF-8 string NOT INCLUDING the
 * termination.
 */
size_t krb5int_utf8_bytes(const char *p)
{
    size_t bytes;

    for (bytes = 0; p[bytes]; bytes++)
        ;

    return bytes;
}

size_t krb5int_utf8_chars(const char *p)
{
    /* could be optimized and could check for invalid sequences */
    size_t chars = 0;

    for ( ; *p ; KRB5_UTF8_INCR(p))
        chars++;

    return chars;
}

size_t krb5int_utf8c_chars(const char *p, size_t length)
{
    /* could be optimized and could check for invalid sequences */
    size_t chars = 0;
    const char *end = p + length;

    for ( ; p < end; KRB5_UTF8_INCR(p))
        chars++;

    return chars;
}

/* return offset to next character */
int krb5int_utf8_offset(const char *p)
{
    return KRB5_UTF8_NEXT(p) - p;
}

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

int krb5int_utf8_charlen(const char *p)
{
    if (!(*p & 0x80))
        return 1;

    return krb5int_utf8_lentab[*(const unsigned char *)p ^ 0x80];
}

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

int krb5int_utf8_charlen2(const char *p)
{
    int i = KRB5_UTF8_CHARLEN(p);

    if (i > 2) {
        if (!(krb5int_utf8_mintab[*p & 0x1f] & p[1]))
            i = 0;
    }

    return i;
}

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

int krb5int_utf8_to_ucs2(const char *p, krb5_ucs2 *out)
{
    krb5_ucs4 ch;

    *out = 0;
    if (krb5int_utf8_to_ucs4(p, &ch) == -1 || ch > 0xFFFF)
        return -1;
    *out = (krb5_ucs2) ch;
    return 0;
}

/* conv UCS-2 to UTF-8, not used */
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

size_t krb5int_ucs2_to_utf8(krb5_ucs2 c, char *buf)
{
    return krb5int_ucs4_to_utf8((krb5_ucs4)c, buf);
}

/*
 * Advance to the next UTF-8 character
 *
 * Ignores length of multibyte character, instead rely on
 * continuation markers to find start of next character.
 * This allows for "resyncing" of when invalid characters
 * are provided provided the start of the next character
 * is appears within the 6 bytes examined.
 */
char *krb5int_utf8_next(const char *p)
{
    int i;
    const unsigned char *u = (const unsigned char *) p;

    if (KRB5_UTF8_ISASCII(u)) {
        return (char *) &p[1];
    }

    for (i = 1; i < 6; i++) {
        if ((u[i] & 0xc0) != 0x80) {
            return (char *) &p[i];
        }
    }

    return (char *) &p[i];
}

/*
 * Advance to the previous UTF-8 character
 *
 * Ignores length of multibyte character, instead rely on
 * continuation markers to find start of next character.
 * This allows for "resyncing" of when invalid characters
 * are provided provided the start of the next character
 * is appears within the 6 bytes examined.
 */
char *krb5int_utf8_prev(const char *p)
{
    int i;
    const unsigned char *u = (const unsigned char *) p;

    for (i = -1; i>-6 ; i--) {
        if ((u[i] & 0xc0 ) != 0x80) {
            return (char *) &p[i];
        }
    }

    return (char *) &p[i];
}

/*
 * Copy one UTF-8 character from src to dst returning
 * number of bytes copied.
 *
 * Ignores length of multibyte character, instead rely on
 * continuation markers to find start of next character.
 * This allows for "resyncing" of when invalid characters
 * are provided provided the start of the next character
 * is appears within the 6 bytes examined.
 */
int krb5int_utf8_copy(char* dst, const char *src)
{
    int i;
    const unsigned char *u = (const unsigned char *) src;

    dst[0] = src[0];

    if (KRB5_UTF8_ISASCII(u)) {
        return 1;
    }

    for (i=1; i<6; i++) {
        if ((u[i] & 0xc0) != 0x80) {
            return i;
        }
        dst[i] = src[i];
    }

    return i;
}

#ifndef UTF8_ALPHA_CTYPE
/*
 * UTF-8 ctype routines
 * Only deals with characters < 0x80 (ie: US-ASCII)
 */

int krb5int_utf8_isascii(const char * p)
{
    unsigned c = * (const unsigned char *) p;

    return KRB5_ASCII(c);
}

int krb5int_utf8_isdigit(const char * p)
{
    unsigned c = * (const unsigned char *) p;

    if (!KRB5_ASCII(c))
        return 0;

    return KRB5_DIGIT( c );
}

int krb5int_utf8_isxdigit(const char * p)
{
    unsigned c = * (const unsigned char *) p;

    if (!KRB5_ASCII(c))
        return 0;

    return KRB5_HEX(c);
}

int krb5int_utf8_isspace(const char * p)
{
    unsigned c = * (const unsigned char *) p;

    if (!KRB5_ASCII(c))
        return 0;

    switch(c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\v':
    case '\f':
        return 1;
    }

    return 0;
}

/*
 * These are not needed by the C SDK and are
 * not "good enough" for general use.
 */
int krb5int_utf8_isalpha(const char * p)
{
    unsigned c = * (const unsigned char *) p;

    if (!KRB5_ASCII(c))
        return 0;

    return KRB5_ALPHA(c);
}

int krb5int_utf8_isalnum(const char * p)
{
    unsigned c = * (const unsigned char *) p;

    if (!KRB5_ASCII(c))
        return 0;

    return KRB5_ALNUM(c);
}

#if 0
int krb5int_utf8_islower(const char * p)
{
    unsigned c = * (const unsigned char *) p;

    if (!KRB5_ASCII(c))
        return 0;

    return KRB5_LOWER(c);
}

int krb5int_utf8_isupper(const char * p)
{
    unsigned c = * (const unsigned char *) p;

    if (!KRB5_ASCII(c))
        return 0;

    return KRB5_UPPER(c);
}
#endif
#endif


/*
 * UTF-8 string routines
 */

/* like strchr() */
char *krb5int_utf8_strchr(const char *str, const char *chr)
{
    krb5_ucs4 chs, ch;

    if (krb5int_utf8_to_ucs4(chr, &ch) == -1)
        return NULL;
    for ( ; *str != '\0'; KRB5_UTF8_INCR(str)) {
        if (krb5int_utf8_to_ucs4(str, &chs) == 0 && chs == ch)
            return (char *)str;
    }

    return NULL;
}

/* like strcspn() but returns number of bytes, not characters */
size_t krb5int_utf8_strcspn(const char *str, const char *set)
{
    const char *cstr, *cset;
    krb5_ucs4 chstr, chset;

    for (cstr = str; *cstr != '\0'; KRB5_UTF8_INCR(cstr)) {
        for (cset = set; *cset != '\0'; KRB5_UTF8_INCR(cset)) {
            if (krb5int_utf8_to_ucs4(cstr, &chstr) == 0
                && krb5int_utf8_to_ucs4(cset, &chset) == 0 && chstr == chset)
                return cstr - str;
        }
    }

    return cstr - str;
}

/* like strspn() but returns number of bytes, not characters */
size_t krb5int_utf8_strspn(const char *str, const char *set)
{
    const char *cstr, *cset;
    krb5_ucs4 chstr, chset;

    for (cstr = str; *cstr != '\0'; KRB5_UTF8_INCR(cstr)) {
        for (cset = set; ; KRB5_UTF8_INCR(cset)) {
            if (*cset == '\0')
                return cstr - str;
            if (krb5int_utf8_to_ucs4(cstr, &chstr) == 0
                && krb5int_utf8_to_ucs4(cset, &chset) == 0 && chstr == chset)
                break;
        }
    }

    return cstr - str;
}

/* like strpbrk(), replaces strchr() as well */
char *krb5int_utf8_strpbrk(const char *str, const char *set)
{
    const char *cset;
    krb5_ucs4 chstr, chset;

    for ( ; *str != '\0'; KRB5_UTF8_INCR(str)) {
        for (cset = set; *cset != '\0'; KRB5_UTF8_INCR(cset)) {
            if (krb5int_utf8_to_ucs4(str, &chstr) == 0
                && krb5int_utf8_to_ucs4(cset, &chset) == 0 && chstr == chset)
                return (char *)str;
        }
    }

    return NULL;
}

/* like strtok_r(), not strtok() */
char *krb5int_utf8_strtok(char *str, const char *sep, char **last)
{
    char *begin;
    char *end;

    if (last == NULL)
        return NULL;

    begin = str ? str : *last;

    begin += krb5int_utf8_strspn(begin, sep);

    if (*begin == '\0') {
        *last = NULL;
        return NULL;
    }

    end = &begin[krb5int_utf8_strcspn(begin, sep)];

    if (*end != '\0') {
        char *next = KRB5_UTF8_NEXT(end);
        *end = '\0';
        end = next;
    }

    *last = end;

    return begin;
}
