/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/sname_match.c - krb5_sname_match API function */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "int-proto.h"

krb5_boolean KRB5_CALLCONV
krb5_sname_match(krb5_context context, krb5_const_principal matching,
                 krb5_const_principal princ)
{
    if (matching == NULL)
        return TRUE;

    if (matching->type != KRB5_NT_SRV_HST || matching->length != 2)
        return krb5_principal_compare(context, matching, princ);

    if (princ->length != 2)
        return FALSE;

    /* Check the realm if present in matching. */
    if (matching->realm.length != 0 && !data_eq(matching->realm, princ->realm))
        return FALSE;

    /* Check the service name (must be present in matching). */
    if (!data_eq(matching->data[0], princ->data[0]))
        return FALSE;

    /* Check the hostname if present in matching and not ignored. */
    if (matching->data[1].length != 0 && !context->ignore_acceptor_hostname &&
        !data_eq(matching->data[1], princ->data[1]))
        return FALSE;

    /* All elements match. */
    return TRUE;
}

krb5_boolean
k5_sname_wildcard_host(krb5_context context, krb5_const_principal mprinc)
{
    if (mprinc == NULL)
        return TRUE;

    if (mprinc->type != KRB5_NT_SRV_HST || mprinc->length != 2)
        return FALSE;

    return context->ignore_acceptor_hostname || mprinc->data[1].length == 0;
}
