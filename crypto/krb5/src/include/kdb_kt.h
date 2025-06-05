/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/kdb_kt.h - KDC keytab declarations */
/*
 * Copyright 1997 by the Massachusetts Institute of Technology.
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

#ifndef KRB5_KDB5_KT_H
#define KRB5_KDB5_KT_H

#include "kdb.h"

extern struct _krb5_kt_ops krb5_kt_kdb_ops;

krb5_error_code krb5_ktkdb_resolve (krb5_context, const char *, krb5_keytab *);

krb5_error_code krb5_ktkdb_set_context(krb5_context);

#endif /* KRB5_KDB5_DBM__ */
