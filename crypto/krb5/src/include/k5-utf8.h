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
 * <https://www.OpenLDAP.org/license.html>.
 */
/*
 * Copyright (C) 2000 Novell, Inc. All Rights Reserved.
 *
 * THIS WORK IS SUBJECT TO U.S. AND INTERNATIONAL COPYRIGHT LAWS AND TREATIES.
 * USE, MODIFICATION, AND REDISTRIBUTION OF THIS WORK IS SUBJECT TO VERSION
 * 2.0.1 OF THE OPENLDAP PUBLIC LICENSE, A COPY OF WHICH IS AVAILABLE AT
 * HTTPS://WWW.OPENLDAP.ORG/LICENSE.HTML OR IN THE FILE "LICENSE" IN THE
 * TOP-LEVEL DIRECTORY OF THE DISTRIBUTION. ANY USE OR EXPLOITATION OF THIS
 * WORK OTHER THAN AS AUTHORIZED IN VERSION 2.0.1 OF THE OPENLDAP PUBLIC
 * LICENSE, OR OTHER PRIOR WRITTEN CONSENT FROM NOVELL, COULD SUBJECT THE
 * PERPETRATOR TO CRIMINAL AND CIVIL LIABILITY.
 */
/* This work is part of OpenLDAP Software <https://www.openldap.org/>. */

#ifndef K5_UTF8_H
#define K5_UTF8_H

#include "k5-platform.h"

typedef uint16_t krb5_ucs2;
typedef uint32_t krb5_ucs4;

int krb5int_utf8_to_ucs4(const char *p, krb5_ucs4 *out);
size_t krb5int_ucs4_to_utf8(krb5_ucs4 c, char *buf);

/*
 * Convert a little-endian UTF-16 string to an allocated null-terminated UTF-8
 * string.  nbytes is the length of ucs2bytes in bytes, and must be an even
 * number.  Return EINVAL on invalid input, ENOMEM on out of memory, or 0 on
 * success.
 */
int k5_utf16le_to_utf8(const uint8_t *utf16bytes, size_t nbytes,
                       char **utf8_out);

/*
 * Convert a UTF-8 string to an allocated little-endian UTF-16 string.  The
 * resulting length is in bytes and will always be even.  Return EINVAL on
 * invalid input, ENOMEM on out of memory, or 0 on success.
 */
int k5_utf8_to_utf16le(const char *utf8, uint8_t **utf16_out,
                       size_t *nbytes_out);

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

/*
 * these macros assume 'x' is an ASCII x
 * and assume the "C" locale
 */
#define KRB5_UPPER(c)           ((c) >= 'A' && (c) <= 'Z')

#endif /* K5_UTF8_H */
