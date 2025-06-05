/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/vfy_increds.c - Verify initial credentials with keytab */
/*
 * Copyright (C) 1998, 2011, 2012 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "int-proto.h"

/* Return true if configuration demands that a keytab be present.  (By default
 * verification will be skipped if no keytab exists.) */
static krb5_boolean
nofail(krb5_context context, krb5_verify_init_creds_opt *options,
       krb5_creds *creds)
{
    int val;

    if (options &&
        (options->flags & KRB5_VERIFY_INIT_CREDS_OPT_AP_REQ_NOFAIL))
        return (options->ap_req_nofail != 0);
    if (krb5int_libdefault_boolean(context, &creds->client->realm,
                                   KRB5_CONF_VERIFY_AP_REQ_NOFAIL,
                                   &val) == 0)
        return (val != 0);
    return FALSE;
}

static krb5_error_code
copy_creds_except(krb5_context context, krb5_ccache incc,
                  krb5_ccache outcc, krb5_principal princ)
{
    krb5_error_code ret, ret2;
    krb5_cc_cursor cur = NULL;
    krb5_creds creds;

    ret = krb5_cc_start_seq_get(context, incc, &cur);
    if (ret)
        return ret;

    while (!(ret = krb5_cc_next_cred(context, incc, &cur, &creds))) {
        if (!krb5_principal_compare(context, princ, creds.server))
            ret = krb5_cc_store_cred(context, outcc, &creds);
        krb5_free_cred_contents(context, &creds);
        if (ret)
            break;
    }

    ret2 = krb5_cc_end_seq_get(context, incc, &cur);
    return (ret == KRB5_CC_END) ? ret2 : ret;
}

static krb5_error_code
get_vfy_cred(krb5_context context, krb5_creds *creds, krb5_principal server,
             krb5_keytab keytab, krb5_ccache *ccache_arg)
{
    krb5_error_code ret;
    krb5_ccache ccache = NULL, retcc = NULL;
    krb5_creds in_creds, *out_creds = NULL;
    krb5_auth_context authcon = NULL;
    krb5_data ap_req = empty_data();

    /* If the creds are for the server principal, we're set, just do a mk_req.
     * Otherwise, do a get_credentials first. */
    if (krb5_principal_compare(context, server, creds->server)) {
        /* Make an ap-req. */
        ret = krb5_mk_req_extended(context, &authcon, 0, NULL, creds, &ap_req);
        if (ret)
            goto cleanup;
    } else {
        /*
         * This is unclean, but it's the easiest way without ripping the
         * library into very small pieces.  store the client's initial cred
         * in a memory ccache, then call the library.  Later, we'll copy
         * everything except the initial cred into the ccache we return to
         * the user.  A clean implementation would involve library
         * internals with a coherent idea of "in" and "out".
         */

        /* Insert the initial cred into the ccache. */
        ret = krb5_cc_new_unique(context, "MEMORY", NULL, &ccache);
        if (ret)
            goto cleanup;
        ret = krb5_cc_initialize(context, ccache, creds->client);
        if (ret)
            goto cleanup;
        ret = krb5_cc_store_cred(context, ccache, creds);
        if (ret)
            goto cleanup;

        /* Get credentials with get_creds. */
        memset(&in_creds, 0, sizeof(in_creds));
        in_creds.client = creds->client;
        in_creds.server = server;
        ret = krb5_timeofday(context, &in_creds.times.endtime);
        if (ret)
            goto cleanup;
        in_creds.times.endtime = ts_incr(in_creds.times.endtime, 5 * 60);
        ret = krb5_get_credentials(context, 0, ccache, &in_creds, &out_creds);
        if (ret)
            goto cleanup;

        /* Make an ap-req. */
        ret = krb5_mk_req_extended(context, &authcon, 0, NULL, out_creds,
                                   &ap_req);
        if (ret)
            goto cleanup;
    }

    /* Wipe the auth context created by mk_req. */
    if (authcon) {
        krb5_auth_con_free(context, authcon);
        authcon = NULL;
    }

    /* Build an auth context that won't bother with replay checks -- it's
     * not as if we're going to mount a replay attack on ourselves here. */
    ret = krb5_auth_con_init(context, &authcon);
    if (ret)
        goto cleanup;
    ret = krb5_auth_con_setflags(context, authcon, 0);
    if (ret)
        goto cleanup;

    /* Verify the ap_req. */
    ret = krb5_rd_req(context, &authcon, &ap_req, server, keytab, NULL, NULL);
    if (ret)
        goto cleanup;

    /* If we get this far, then the verification succeeded.  We can
     * still fail if the library stuff here fails, but that's it. */
    if (ccache_arg != NULL && ccache != NULL) {
        if (*ccache_arg == NULL) {
            ret = krb5_cc_resolve(context, "MEMORY:rd_req2", &retcc);
            if (ret)
                goto cleanup;
            ret = krb5_cc_initialize(context, retcc, creds->client);
            if (ret)
                goto cleanup;
            ret = copy_creds_except(context, ccache, retcc, creds->server);
            if (ret)
                goto cleanup;
            *ccache_arg = retcc;
            retcc = NULL;
        } else {
            ret = copy_creds_except(context, ccache, *ccache_arg, server);
        }
    }

cleanup:
    if (retcc != NULL)
        krb5_cc_destroy(context, retcc);
    if (ccache != NULL)
        krb5_cc_destroy(context, ccache);
    krb5_free_creds(context, out_creds);
    krb5_auth_con_free(context, authcon);
    krb5_free_data_contents(context, &ap_req);
    return ret;
}

/* Free the principals in plist and plist itself. */
static void
free_princ_list(krb5_context context, krb5_principal *plist)
{
    size_t i;

    if (plist == NULL)
        return;
    for (i = 0; plist[i] != NULL; i++)
        krb5_free_principal(context, plist[i]);
    free(plist);
}

/* Add princ to plist if it isn't already there. */
static krb5_error_code
add_princ_list(krb5_context context, krb5_const_principal princ,
               krb5_principal **plist)
{
    size_t i;
    krb5_principal *newlist;

    /* Check if princ is already in plist, and count the elements. */
    for (i = 0; (*plist) != NULL && (*plist)[i] != NULL; i++) {
        if (krb5_principal_compare(context, princ, (*plist)[i]))
            return 0;
    }

    newlist = realloc(*plist, (i + 2) * sizeof(*newlist));
    if (newlist == NULL)
        return ENOMEM;
    *plist = newlist;
    newlist[i] = newlist[i + 1] = NULL; /* terminate the list */
    return krb5_copy_principal(context, princ, &newlist[i]);
}

/* Return a list of all unique host service princs in keytab. */
static krb5_error_code
get_host_princs_from_keytab(krb5_context context, krb5_keytab keytab,
                            krb5_principal **princ_list_out)
{
    krb5_error_code ret;
    krb5_kt_cursor cursor;
    krb5_keytab_entry kte;
    krb5_principal *plist = NULL, p;

    *princ_list_out = NULL;

    ret = krb5_kt_start_seq_get(context, keytab, &cursor);
    if (ret)
        goto cleanup;

    while ((ret = krb5_kt_next_entry(context, keytab, &kte, &cursor)) == 0) {
        p = kte.principal;
        if (p->length == 2 && data_eq_string(p->data[0], "host"))
            ret = add_princ_list(context, p, &plist);
        krb5_kt_free_entry(context, &kte);
        if (ret)
            break;
    }
    (void)krb5_kt_end_seq_get(context, keytab, &cursor);
    if (ret == KRB5_KT_END)
        ret = 0;
    if (ret)
        goto cleanup;

    *princ_list_out = plist;
    plist = NULL;

cleanup:
    free_princ_list(context, plist);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_verify_init_creds(krb5_context context, krb5_creds *creds,
                       krb5_principal server, krb5_keytab keytab,
                       krb5_ccache *ccache,
                       krb5_verify_init_creds_opt *options)
{
    krb5_error_code ret;
    krb5_principal *host_princs = NULL;
    krb5_keytab defkeytab = NULL;
    krb5_keytab_entry kte;
    krb5_boolean have_keys = FALSE;
    size_t i;

    if (keytab == NULL) {
        ret = krb5_kt_default(context, &defkeytab);
        if (ret)
            goto cleanup;
        keytab = defkeytab;
    }

    if (server != NULL) {
        /* Check if server exists in keytab first. */
        ret = krb5_kt_get_entry(context, keytab, server, 0, 0, &kte);
        if (ret)
            goto cleanup;
        krb5_kt_free_entry(context, &kte);
        have_keys = TRUE;
        ret = get_vfy_cred(context, creds, server, keytab, ccache);
    } else {
        /* Try using the host service principals from the keytab. */
        if (keytab->ops->start_seq_get == NULL) {
            ret = EINVAL;
            goto cleanup;
        }
        ret = get_host_princs_from_keytab(context, keytab, &host_princs);
        if (ret)
            goto cleanup;
        if (host_princs == NULL) {
            ret = KRB5_KT_NOTFOUND;
            goto cleanup;
        }
        have_keys = TRUE;

        /* Try all host principals until one succeeds or they all fail. */
        for (i = 0; host_princs[i] != NULL; i++) {
            ret = get_vfy_cred(context, creds, host_princs[i], keytab, ccache);
            if (ret == 0)
                break;
        }
    }

cleanup:
    /* If we have no key to verify with, pretend to succeed unless
     * configuration directs otherwise. */
    if (!have_keys && !nofail(context, options, creds))
        ret = 0;

    if (defkeytab != NULL)
        krb5_kt_close(context, defkeytab);
    free_princ_list(context, host_princs);

    return ret;
}
