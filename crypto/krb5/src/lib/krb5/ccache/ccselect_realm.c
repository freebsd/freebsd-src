/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/ccselect_realm.c - realm ccselect module */
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
#include "cc-int.h"
#include <krb5/ccselect_plugin.h>

static krb5_error_code
realm_init(krb5_context context, krb5_ccselect_moddata *data_out,
           int *priority_out)
{
    *data_out = NULL;
    *priority_out = KRB5_CCSELECT_PRIORITY_HEURISTIC;
    return 0;
}

static krb5_error_code
realm_choose(krb5_context context, krb5_ccselect_moddata data,
             krb5_principal server, krb5_ccache *cache_out,
             krb5_principal *princ_out)
{
    krb5_error_code ret;
    krb5_cccol_cursor cursor;
    krb5_ccache cache;
    krb5_principal princ;

    *cache_out = NULL;
    *princ_out = NULL;

    if (krb5_is_referral_realm(&server->realm))
        return KRB5_PLUGIN_NO_HANDLE;

    /* Scan the collection for a cache with a client principal in the same
     * realm as the server principal. */
    ret = krb5_cccol_cursor_new(context, &cursor);
    if (ret)
        return ret;
    while ((ret = krb5_cccol_cursor_next(context, cursor, &cache)) == 0 &&
           cache != NULL) {
        ret = krb5_cc_get_principal(context, cache, &princ);
        if (ret == 0) {
            if (data_eq(princ->realm, server->realm))
                break;
            krb5_free_principal(context, princ);
        }
        krb5_cc_close(context, cache);
    }
    krb5_cccol_cursor_free(context, &cursor);
    if (ret)
        return ret;

    if (cache == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    *cache_out = cache;
    *princ_out = princ;
    return 0;
}

krb5_error_code
ccselect_realm_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable)
{
    krb5_ccselect_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_ccselect_vtable)vtable;
    vt->name = "realm";
    vt->init = realm_init;
    vt->choose = realm_choose;
    return 0;
}
