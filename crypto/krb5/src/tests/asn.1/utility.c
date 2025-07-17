/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/asn.1/utility.c */
/*
 * Copyright (C) 1994 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#include "utility.h"
#include "krb5.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

krb5int_access acc;

char hexchar (const unsigned int digit);

void *
ealloc(size_t size)
{
    void *ptr = calloc(1, size);

    if (ptr == NULL)
        abort();
    return ptr;
}

char *
estrdup(const char *str)
{
    char *newstr = strdup(str);

    if (newstr == NULL)
        abort();
    return newstr;
}

void
asn1_krb5_data_unparse(const krb5_data *code, char **s)
{
    if (*s != NULL) free(*s);

    if (code==NULL) {
        *s = estrdup("<NULL>");
    } else if (code->data == NULL || ((int) code->length) <= 0) {
        *s = estrdup("<EMPTY>");
    } else {
        unsigned int i;

        *s = ealloc(3 * code->length);
        for (i = 0; i < code->length; i++) {
            (*s)[3*i] = hexchar((unsigned char) (((code->data)[i]&0xF0)>>4));
            (*s)[3*i+1] = hexchar((unsigned char) ((code->data)[i]&0x0F));
            (*s)[3*i+2] = ' ';
        }
        (*s)[3*(code->length)-1] = '\0';
    }
}

char
hexchar(const unsigned int digit)
{
    if (digit<=9)
        return '0'+digit;
    else if (digit<=15)
        return 'A'+digit-10;
    else
        return 'X';
}

void
krb5_data_parse(krb5_data *d, const char *s)
{
    d->length = strlen(s);
    d->data = ealloc(d->length);
    memcpy(d->data, s, d->length);
}

krb5_error_code
krb5_data_hex_parse(krb5_data *d, const char *s)
{
    int lo;
    long v;
    const char *cp;
    char *dp;
    char buf[2];

    d->data = ealloc(strlen(s) / 2 + 1);
    d->length = 0;
    buf[1] = '\0';
    for (lo = 0, dp = d->data, cp = s; *cp; cp++) {
        if (*cp < 0)
            return ASN1_PARSE_ERROR;
        else if (isspace((unsigned char) *cp))
            continue;
        else if (isxdigit((unsigned char) *cp)) {
            buf[0] = *cp;
            v = strtol(buf, NULL, 16);
        } else
            return ASN1_PARSE_ERROR;
        if (lo) {
            *dp++ |= v;
            lo = 0;
        } else {
            *dp = v << 4;
            lo = 1;
        }
    }

    d->length = dp - d->data;
    return 0;
}

void
init_access(const char *progname)
{
    krb5_error_code ret;
    ret = krb5int_accessor(&acc, KRB5INT_ACCESS_VERSION);
    if (ret) {
        com_err(progname, ret, "while initializing accessor");
        exit(1);
    }
}
