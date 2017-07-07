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

static krb5_error_code KRB5_CALLCONV
krb5_rc_none_init(krb5_context ctx, krb5_rcache rc, krb5_deltat d)
{
    return 0;
}
#define krb5_rc_none_recover_or_init krb5_rc_none_init

static krb5_error_code KRB5_CALLCONV
krb5_rc_none_noargs(krb5_context ctx, krb5_rcache rc)
{
    return 0;
}
#define krb5_rc_none_recover    krb5_rc_none_noargs
#define krb5_rc_none_expunge    krb5_rc_none_noargs

static krb5_error_code KRB5_CALLCONV
krb5_rc_none_close(krb5_context ctx, krb5_rcache rc)
{
    free (rc);
    return 0;
}
#define krb5_rc_none_destroy    krb5_rc_none_close

static krb5_error_code KRB5_CALLCONV
krb5_rc_none_store(krb5_context ctx, krb5_rcache rc, krb5_donot_replay *r)
{
    return 0;
}

static krb5_error_code KRB5_CALLCONV
krb5_rc_none_get_span(krb5_context ctx, krb5_rcache rc, krb5_deltat *d)
{
    return 0;
}

static char * KRB5_CALLCONV
krb5_rc_none_get_name(krb5_context ctx, krb5_rcache rc)
{
    return "";
}

static krb5_error_code KRB5_CALLCONV
krb5_rc_none_resolve(krb5_context ctx, krb5_rcache rc, char *name)
{
    rc->data = "none";
    return 0;
}

const krb5_rc_ops krb5_rc_none_ops = {
    0,
    "none",
    krb5_rc_none_init,
    krb5_rc_none_recover,
    krb5_rc_none_recover_or_init,
    krb5_rc_none_destroy,
    krb5_rc_none_close,
    krb5_rc_none_store,
    krb5_rc_none_expunge,
    krb5_rc_none_get_span,
    krb5_rc_none_get_name,
    krb5_rc_none_resolve
};
