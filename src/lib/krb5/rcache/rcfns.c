/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rcfns.c */
/*
 * Copyright 2001 by the Massachusetts Institute of Technology.
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
 * Dispatch methods for replay cache code.
 */

#include "k5-int.h"
#include "rc-int.h"

krb5_error_code KRB5_CALLCONV
krb5_rc_initialize (krb5_context context, krb5_rcache id, krb5_deltat span)
{
    return krb5_x(id->ops->init,(context, id, span));
}

krb5_error_code KRB5_CALLCONV
krb5_rc_recover_or_initialize (krb5_context context, krb5_rcache id,
                               krb5_deltat span)
{
    return krb5_x(id->ops->recover_or_init,(context, id, span));
}

krb5_error_code KRB5_CALLCONV
krb5_rc_recover (krb5_context context, krb5_rcache id)
{
    return krb5_x((id)->ops->recover,(context, id));
}

krb5_error_code KRB5_CALLCONV
krb5_rc_destroy (krb5_context context, krb5_rcache id)
{
    return krb5_x((id)->ops->destroy,(context, id));
}

krb5_error_code KRB5_CALLCONV
krb5_rc_close (krb5_context context, krb5_rcache id)
{
    return krb5_x((id)->ops->close,(context, id));
}

krb5_error_code KRB5_CALLCONV
krb5_rc_store (krb5_context context, krb5_rcache id,
               krb5_donot_replay *dontreplay)
{
    return krb5_x((id)->ops->store,(context, id, dontreplay));
}

krb5_error_code KRB5_CALLCONV
krb5_rc_expunge (krb5_context context, krb5_rcache id)
{
    return krb5_x((id)->ops->expunge,(context, id));
}

krb5_error_code KRB5_CALLCONV
krb5_rc_get_lifespan (krb5_context context, krb5_rcache id,
                      krb5_deltat *spanp)
{
    return krb5_x((id)->ops->get_span,(context, id, spanp));
}

char *KRB5_CALLCONV
krb5_rc_get_name (krb5_context context, krb5_rcache id)
{
    return krb5_xc((id)->ops->get_name,(context, id));
}

krb5_error_code KRB5_CALLCONV
krb5_rc_resolve (krb5_context context, krb5_rcache id, char *name)
{
    return krb5_x((id)->ops->resolve,(context, id, name));
}
