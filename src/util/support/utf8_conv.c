/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/utf8_conv.c */
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

/* This work is part of OpenLDAP Software <http://www.openldap.org/>. */

/*
 * UTF-8 Conversion Routines
 *
 * These routines convert between Wide Character and UTF-8,
 * or between MultiByte and UTF-8 encodings.
 *
 * Both single character and string versions of the functions are provided.
 * All functions return -1 if the character or string cannot be converted.
 */

#include "k5-platform.h"
#include "k5-utf8.h"
#include "supp-int.h"

static unsigned char mask[] = { 0, 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };

static ssize_t
k5_utf8s_to_ucs2s(krb5_ucs2 *ucs2str,
                  const char *utf8str,
                  size_t count,
                  int little_endian)
{
    size_t ucs2len = 0;
    size_t utflen, i;
    krb5_ucs2 ch;

    /* If input ptr is NULL or empty... */
    if (utf8str == NULL || *utf8str == '\0') {
        if (ucs2str != NULL)
            *ucs2str = 0;

        return 0;
    }

    /* Examine next UTF-8 character.  */
    while (ucs2len < count && *utf8str != '\0') {
        /* Get UTF-8 sequence length from 1st byte */
        utflen = KRB5_UTF8_CHARLEN2(utf8str, utflen);

        if (utflen == 0 || utflen > KRB5_MAX_UTF8_LEN)
            return -1;

        /* First byte minus length tag */
        ch = (krb5_ucs2)(utf8str[0] & mask[utflen]);

        for (i = 1; i < utflen; i++) {
            /* Subsequent bytes must start with 10 */
            if ((utf8str[i] & 0xc0) != 0x80)
                return -1;

            ch <<= 6;                   /* 6 bits of data in each subsequent byte */
            ch |= (krb5_ucs2)(utf8str[i] & 0x3f);
        }

        if (ucs2str != NULL) {
#ifdef K5_BE
#ifndef SWAP16
#define SWAP16(X)       ((((X) << 8) | ((X) >> 8)) & 0xFFFF)
#endif
            if (little_endian)
                ucs2str[ucs2len] = SWAP16(ch);
            else
#endif
                ucs2str[ucs2len] = ch;
        }

        utf8str += utflen;      /* Move to next UTF-8 character */
        ucs2len++;              /* Count number of wide chars stored/required */
    }

    if (ucs2str != NULL && ucs2len < count) {
        /* Add null terminator if there's room in the buffer. */
        ucs2str[ucs2len] = 0;
    }

    return ucs2len;
}

int
krb5int_utf8s_to_ucs2s(const char *utf8s,
                       krb5_ucs2 **ucs2s,
                       size_t *ucs2chars)
{
    ssize_t len;
    size_t chars;

    chars = krb5int_utf8_chars(utf8s);
    *ucs2s = (krb5_ucs2 *)malloc((chars + 1) * sizeof(krb5_ucs2));
    if (*ucs2s == NULL) {
        return ENOMEM;
    }

    len = k5_utf8s_to_ucs2s(*ucs2s, utf8s, chars + 1, 0);
    if (len < 0) {
        free(*ucs2s);
        *ucs2s = NULL;
        return EINVAL;
    }

    if (ucs2chars != NULL) {
        *ucs2chars = chars;
    }

    return 0;
}

int
krb5int_utf8cs_to_ucs2s(const char *utf8s,
                        size_t utf8slen,
                        krb5_ucs2 **ucs2s,
                        size_t *ucs2chars)
{
    ssize_t len;
    size_t chars;

    chars = krb5int_utf8c_chars(utf8s, utf8slen);
    *ucs2s = (krb5_ucs2 *)malloc((chars + 1) * sizeof(krb5_ucs2));
    if (*ucs2s == NULL) {
        return ENOMEM;
    }

    len = k5_utf8s_to_ucs2s(*ucs2s, utf8s, chars, 0);
    if (len < 0) {
        free(*ucs2s);
        *ucs2s = NULL;
        return EINVAL;
    }
    (*ucs2s)[chars] = 0;

    if (ucs2chars != NULL) {
        *ucs2chars = chars;
    }

    return 0;
}

int
krb5int_utf8s_to_ucs2les(const char *utf8s,
                         unsigned char **ucs2les,
                         size_t *ucs2leslen)
{
    ssize_t len;
    size_t chars;

    chars = krb5int_utf8_chars(utf8s);

    *ucs2les = (unsigned char *)malloc((chars + 1) * sizeof(krb5_ucs2));
    if (*ucs2les == NULL) {
        return ENOMEM;
    }

    len = k5_utf8s_to_ucs2s((krb5_ucs2 *)*ucs2les, utf8s, chars + 1, 1);
    if (len < 0) {
        free(*ucs2les);
        *ucs2les = NULL;
        return EINVAL;
    }

    if (ucs2leslen != NULL) {
        *ucs2leslen = chars * sizeof(krb5_ucs2);
    }

    return 0;
}

int
krb5int_utf8cs_to_ucs2les(const char *utf8s,
                          size_t utf8slen,
                          unsigned char **ucs2les,
                          size_t *ucs2leslen)
{
    ssize_t len;
    size_t chars;
    krb5_ucs2 *ucs2s;

    *ucs2les = NULL;

    chars = krb5int_utf8c_chars(utf8s, utf8slen);
    ucs2s = malloc((chars + 1) * sizeof(krb5_ucs2));
    if (ucs2s == NULL)
        return ENOMEM;

    len = k5_utf8s_to_ucs2s(ucs2s, utf8s, chars, 1);
    if (len < 0) {
        free(ucs2s);
        return EINVAL;
    }
    ucs2s[chars] = 0;

    *ucs2les = (unsigned char *)ucs2s;
    if (ucs2leslen != NULL) {
        *ucs2leslen = chars * sizeof(krb5_ucs2);
    }

    return 0;
}

/*-----------------------------------------------------------------------------
  Convert a wide char string to a UTF-8 string.
  No more than 'count' bytes will be written to the output buffer.
  Return the # of bytes written to the output buffer, excl null terminator.

  ucs2len is -1 if the UCS-2 string is NUL terminated, otherwise it is the
  length of the UCS-2 string in characters
*/
static ssize_t
k5_ucs2s_to_utf8s(char *utf8str, const krb5_ucs2 *ucs2str,
                  size_t count, ssize_t ucs2len, int little_endian)
{
    int len = 0;
    int n;
    char *p = utf8str;
    krb5_ucs2 empty = 0, ch;

    if (ucs2str == NULL)        /* Treat input ptr NULL as an empty string */
        ucs2str = &empty;

    if (utf8str == NULL)        /* Just compute size of output, excl null */
    {
        while (ucs2len == -1 ? *ucs2str : --ucs2len >= 0) {
            /* Get UTF-8 size of next wide char */
            ch = *ucs2str++;
#ifdef K5_BE
            if (little_endian)
                ch = SWAP16(ch);
#endif

            n = krb5int_ucs2_to_utf8(ch, NULL);
            if (n < 1 || n > INT_MAX - len)
                return -1;
            len += n;
        }

        return len;
    }

    /* Do the actual conversion. */

    n = 1;                                      /* In case of empty ucs2str */
    while (ucs2len == -1 ? *ucs2str != 0 : --ucs2len >= 0) {
        ch = *ucs2str++;
#ifdef K5_BE
        if (little_endian)
            ch = SWAP16(ch);
#endif

        n = krb5int_ucs2_to_utf8(ch, p);

        if (n < 1)
            break;

        p += n;
        count -= n;                     /* Space left in output buffer */
    }

    /* If not enough room for last character, pad remainder with null
       so that return value = original count, indicating buffer full. */
    if (n == 0) {
        while (count--)
            *p++ = 0;
    }
    /* Add a null terminator if there's room. */
    else if (count)
        *p = 0;

    if (n == -1)                        /* Conversion encountered invalid wide char. */
        return -1;

    /* Return the number of bytes written to output buffer, excl null. */
    return (p - utf8str);
}

int
krb5int_ucs2s_to_utf8s(const krb5_ucs2 *ucs2s,
                       char **utf8s,
                       size_t *utf8slen)
{
    ssize_t len;

    len = k5_ucs2s_to_utf8s(NULL, ucs2s, 0, -1, 0);
    if (len < 0) {
        return EINVAL;
    }

    *utf8s = (char *)malloc((size_t)len + 1);
    if (*utf8s == NULL) {
        return ENOMEM;
    }

    len = k5_ucs2s_to_utf8s(*utf8s, ucs2s, (size_t)len + 1, -1, 0);
    if (len < 0) {
        free(*utf8s);
        *utf8s = NULL;
        return EINVAL;
    }

    if (utf8slen != NULL) {
        *utf8slen = len;
    }

    return 0;
}

int
krb5int_ucs2les_to_utf8s(const unsigned char *ucs2les,
                         char **utf8s,
                         size_t *utf8slen)
{
    ssize_t len;

    len = k5_ucs2s_to_utf8s(NULL, (krb5_ucs2 *)ucs2les, 0, -1, 1);
    if (len < 0)
        return EINVAL;

    *utf8s = (char *)malloc((size_t)len + 1);
    if (*utf8s == NULL) {
        return ENOMEM;
    }

    len = k5_ucs2s_to_utf8s(*utf8s, (krb5_ucs2 *)ucs2les, (size_t)len + 1, -1, 1);
    if (len < 0) {
        free(*utf8s);
        *utf8s = NULL;
        return EINVAL;
    }

    if (utf8slen != NULL) {
        *utf8slen = len;
    }

    return 0;
}

int
krb5int_ucs2cs_to_utf8s(const krb5_ucs2 *ucs2s,
                        size_t ucs2slen,
                        char **utf8s,
                        size_t *utf8slen)
{
    ssize_t len;

    if (ucs2slen > SSIZE_MAX)
        return ERANGE;

    len = k5_ucs2s_to_utf8s(NULL, (krb5_ucs2 *)ucs2s, 0,
                            (ssize_t)ucs2slen, 0);
    if (len < 0)
        return EINVAL;

    *utf8s = (char *)malloc((size_t)len + 1);
    if (*utf8s == NULL) {
        return ENOMEM;
    }

    len = k5_ucs2s_to_utf8s(*utf8s, (krb5_ucs2 *)ucs2s, (size_t)len,
                            (ssize_t)ucs2slen, 0);
    if (len < 0) {
        free(*utf8s);
        *utf8s = NULL;
        return EINVAL;
    }
    (*utf8s)[len] = '\0';

    if (utf8slen != NULL) {
        *utf8slen = len;
    }

    return 0;
}

int
krb5int_ucs2lecs_to_utf8s(const unsigned char *ucs2les,
                          size_t ucs2leslen,
                          char **utf8s,
                          size_t *utf8slen)
{
    ssize_t len;

    if (ucs2leslen > SSIZE_MAX)
        return ERANGE;

    len = k5_ucs2s_to_utf8s(NULL, (krb5_ucs2 *)ucs2les, 0,
                            (ssize_t)ucs2leslen, 1);
    if (len < 0)
        return EINVAL;

    *utf8s = (char *)malloc((size_t)len + 1);
    if (*utf8s == NULL) {
        return ENOMEM;
    }

    len = k5_ucs2s_to_utf8s(*utf8s, (krb5_ucs2 *)ucs2les, (size_t)len,
                            (ssize_t)ucs2leslen, 1);
    if (len < 0) {
        free(*utf8s);
        *utf8s = NULL;
        return EINVAL;
    }
    (*utf8s)[len] = '\0';

    if (utf8slen != NULL) {
        *utf8slen = len;
    }

    return 0;
}
