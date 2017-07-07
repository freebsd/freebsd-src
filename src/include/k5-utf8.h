/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2008 by the Massachusetts Institute of Technology,
 * Cambridge, MA, USA.  All Rights Reserved.
 *
 * This software is being provided to you, the LICENSEE, by the
 * Massachusetts Institute of Technology (M.I.T.) under the following
 * license.  By obtaining, using and/or copying this software, you agree
 * that you have read, understood, and will comply with these terms and
 * conditions:
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify and distribute
 * this software and its documentation for any purpose and without fee or
 * royalty is hereby granted, provided that you agree to comply with the
 * following copyright notice and statements, including the disclaimer, and
 * that the same appear on ALL copies of the software and documentation,
 * including modifications that you make for internal use or for
 * distribution:
 *
 * THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS
 * OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 * limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF
 * THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY
 * PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 *
 * The name of the Massachusetts Institute of Technology or M.I.T. may NOT
 * be used in advertising or publicity pertaining to distribution of the
 * software.  Title to copyright in this software and any associated
 * documentation shall at all times remain with M.I.T., and USER agrees to
 * preserve same.
 *
 * Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 */
/*
 * Copyright 1998-2008 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/*
 * Copyright (C) 2000 Novell, Inc. All Rights Reserved.
 *
 * THIS WORK IS SUBJECT TO U.S. AND INTERNATIONAL COPYRIGHT LAWS AND TREATIES.
 * USE, MODIFICATION, AND REDISTRIBUTION OF THIS WORK IS SUBJECT TO VERSION
 * 2.0.1 OF THE OPENLDAP PUBLIC LICENSE, A COPY OF WHICH IS AVAILABLE AT
 * HTTP://WWW.OPENLDAP.ORG/LICENSE.HTML OR IN THE FILE "LICENSE" IN THE
 * TOP-LEVEL DIRECTORY OF THE DISTRIBUTION. ANY USE OR EXPLOITATION OF THIS
 * WORK OTHER THAN AS AUTHORIZED IN VERSION 2.0.1 OF THE OPENLDAP PUBLIC
 * LICENSE, OR OTHER PRIOR WRITTEN CONSENT FROM NOVELL, COULD SUBJECT THE
 * PERPETRATOR TO CRIMINAL AND CIVIL LIABILITY.
 */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>. */

#ifndef K5_UTF8_H
#define K5_UTF8_H

#include "k5-platform.h"

typedef uint16_t krb5_ucs2;
typedef uint32_t krb5_ucs4;

#define KRB5_MAX_UTF8_LEN   (sizeof(krb5_ucs2) * 3/2)

int krb5int_utf8_to_ucs2(const char *p, krb5_ucs2 *out);
size_t krb5int_ucs2_to_utf8(krb5_ucs2 c, char *buf);

int krb5int_utf8_to_ucs4(const char *p, krb5_ucs4 *out);
size_t krb5int_ucs4_to_utf8(krb5_ucs4 c, char *buf);

int
krb5int_ucs2s_to_utf8s(const krb5_ucs2 *ucs2s,
                       char **utf8s,
                       size_t *utf8slen);

int
krb5int_ucs2cs_to_utf8s(const krb5_ucs2 *ucs2s,
                        size_t ucs2slen,
                        char **utf8s,
                        size_t *utf8slen);

int
krb5int_ucs2les_to_utf8s(const unsigned char *ucs2les,
                         char **utf8s,
                         size_t *utf8slen);

int
krb5int_ucs2lecs_to_utf8s(const unsigned char *ucs2les,
                          size_t ucs2leslen,
                          char **utf8s,
                          size_t *utf8slen);

int
krb5int_utf8s_to_ucs2s(const char *utf8s,
                       krb5_ucs2 **ucs2s,
                       size_t *ucs2chars);

int
krb5int_utf8cs_to_ucs2s(const char *utf8s,
                        size_t utf8slen,
                        krb5_ucs2 **ucs2s,
                        size_t *ucs2chars);

int
krb5int_utf8s_to_ucs2les(const char *utf8s,
                         unsigned char **ucs2les,
                         size_t *ucs2leslen);

int
krb5int_utf8cs_to_ucs2les(const char *utf8s,
                          size_t utf8slen,
                          unsigned char **ucs2les,
                          size_t *ucs2leslen);

/* returns the number of bytes in the UTF-8 string */
size_t krb5int_utf8_bytes(const char *);
/* returns the number of UTF-8 characters in the string */
size_t krb5int_utf8_chars(const char *);
/* returns the number of UTF-8 characters in the counted string */
size_t krb5int_utf8c_chars(const char *, size_t);
/* returns the length (in bytes) of the UTF-8 character */
int krb5int_utf8_offset(const char *);
/* returns the length (in bytes) indicated by the UTF-8 character */
int krb5int_utf8_charlen(const char *);

/* returns the length (in bytes) indicated by the UTF-8 character
 * also checks that shortest possible encoding was used
 */
int krb5int_utf8_charlen2(const char *);

/* copies a UTF-8 character and returning number of bytes copied */
int krb5int_utf8_copy(char *, const char *);

/* returns pointer of next UTF-8 character in string */
char *krb5int_utf8_next( const char *);
/* returns pointer of previous UTF-8 character in string */
char *krb5int_utf8_prev( const char *);

/* primitive ctype routines -- not aware of non-ascii characters */
int krb5int_utf8_isascii( const char *);
int krb5int_utf8_isalpha( const char *);
int krb5int_utf8_isalnum( const char *);
int krb5int_utf8_isdigit( const char *);
int krb5int_utf8_isxdigit( const char *);
int krb5int_utf8_isspace( const char *);

/* span characters not in set, return bytes spanned */
size_t krb5int_utf8_strcspn( const char* str, const char *set);
/* span characters in set, return bytes spanned */
size_t krb5int_utf8_strspn( const char* str, const char *set);
/* return first occurance of character in string */
char *krb5int_utf8_strchr( const char* str, const char *chr);
/* return first character of set in string */
char *krb5int_utf8_strpbrk( const char* str, const char *set);
/* reentrant tokenizer */
char *krb5int_utf8_strtok( char* sp, const char* sep, char **last);

/* Optimizations */
extern const char krb5int_utf8_lentab[128];
extern const char krb5int_utf8_mintab[32];

#define KRB5_UTF8_BV(p) (*(const unsigned char *)(p))
#define KRB5_UTF8_ISASCII(p) (!(KRB5_UTF8_BV(p) & 0x80))
#define KRB5_UTF8_CHARLEN(p) (KRB5_UTF8_ISASCII(p) ? 1 :                \
                              krb5int_utf8_lentab[KRB5_UTF8_BV(p) ^ 0x80])

/* This is like CHARLEN but additionally validates to make sure
 * the char used the shortest possible encoding.
 * 'l' is used to temporarily hold the result of CHARLEN.
 */
#define KRB5_UTF8_CHARLEN2(p, l) (                                      \
        ((l = KRB5_UTF8_CHARLEN(p)) < 3 ||                              \
         (krb5int_utf8_mintab[KRB5_UTF8_BV(p) & 0x1f] & (p)[1])) ?      \
        l : 0)

#define KRB5_UTF8_OFFSET(p) (KRB5_UTF8_ISASCII(p)               \
                             ? 1 : krb5int_utf8_offset((p)) )

#define KRB5_UTF8_COPY(d,s) (KRB5_UTF8_ISASCII(s)                       \
                             ? (*(d) = *(s), 1) : krb5int_utf8_copy((d),(s)))

#define KRB5_UTF8_NEXT(p) (KRB5_UTF8_ISASCII(p)                         \
                           ? (char *)(p)+1 : krb5int_utf8_next((p)))

#define KRB5_UTF8_INCR(p) ((p) = KRB5_UTF8_NEXT(p))

/* For symmetry */
#define KRB5_UTF8_PREV(p) (krb5int_utf8_prev((p)))
#define KRB5_UTF8_DECR(p) ((p)=KRB5_UTF8_PREV((p)))

/*
 * these macros assume 'x' is an ASCII x
 * and assume the "C" locale
 */
#define KRB5_ASCII(c)           (!((c) & 0x80))
#define KRB5_SPACE(c)           ((c) == ' ' || (c) == '\t' || (c) == '\n')
#define KRB5_DIGIT(c)           ((c) >= '0' && (c) <= '9')
#define KRB5_LOWER(c)           ((c) >= 'a' && (c) <= 'z')
#define KRB5_UPPER(c)           ((c) >= 'A' && (c) <= 'Z')
#define KRB5_ALPHA(c)           (KRB5_LOWER(c) || KRB5_UPPER(c))
#define KRB5_ALNUM(c)           (KRB5_ALPHA(c) || KRB5_DIGIT(c))

#define KRB5_LDH(c)             (KRB5_ALNUM(c) || (c) == '-')

#define KRB5_HEXLOWER(c)        ((c) >= 'a' && (c) <= 'f')
#define KRB5_HEXUPPER(c)        ((c) >= 'A' && (c) <= 'F')
#define KRB5_HEX(c)             (KRB5_DIGIT(c) ||                       \
                                 KRB5_HEXLOWER(c) || KRB5_HEXUPPER(c))

#endif /* K5_UTF8_H */
