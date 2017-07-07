/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/asn.1/utility.h */
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

#ifndef __UTILITY_H__
#define __UTILITY_H__

#include "k5-int.h"
#include "krbasn1.h"
#include "asn1buf.h"

/* Aborts on failure.  ealloc returns zero-filled memory. */
void *ealloc(size_t size);
char *estrdup(const char *str);

void asn1_krb5_data_unparse(const krb5_data *code, char **s);
/* modifies  *s;
   effects   Instantiates *s with a string representation of the series
              of hex octets in *code.  (e.g. "02 02 00 7F")  If code==NULL,
              the string rep is "<NULL>".  If code is empty (it contains no
              data or has length <= 0), the string rep is "<EMPTY>".
             If *s is non-NULL, then its currently-allocated storage
              will be freed prior to the instantiation.
             Returns ENOMEM or the string rep cannot be created. */

void krb5_data_parse(krb5_data *d, const char *s);
/* effects  Parses character string *s into krb5_data *d. */

asn1_error_code krb5_data_hex_parse(krb5_data *d, const char *s);
/* requires  *s is the string representation of a sequence of
              hexadecimal octets.  (e.g. "02 01 00")
   effects  Parses *s into krb5_data *d. */

void asn1buf_print(const asn1buf *buf);

extern krb5int_access acc;
extern void init_access(const char *progname);

#endif
