/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc_none.c */
/*
 * Copyright 2004 by the Massachusetts Institute of Technology.
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
 *
 * replay cache no-op implementation
 */

#include "k5-int.h"
#include "rc-int.h"

static krb5_error_code
none_resolve(krb5_context ctx, const char *residual, void **rcdata_out)
{
    *rcdata_out = NULL;
    return 0;
}

static void
none_close(krb5_context ctx, void *rcdata)
{
}

static krb5_error_code
none_store(krb5_context ctx, void *rcdata, const krb5_data *tag)
{
    return 0;
}

const krb5_rc_ops k5_rc_none_ops = {
    "none",
    none_resolve,
    none_close,
    none_store
};
