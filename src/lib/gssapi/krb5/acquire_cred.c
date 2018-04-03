/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2000, 2007-2010 by the Massachusetts Institute of Technology.
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
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "k5-int.h"
#include "gssapiP_krb5.h"
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef USE_LEASH
#ifdef _WIN64
#define LEASH_DLL "leashw64.dll"
#else
#define LEASH_DLL "leashw32.dll"
#endif
static void (*pLeash_AcquireInitialTicketsIfNeeded)(krb5_context,krb5_principal,char*,int) = NULL;
static HANDLE hLeashDLL = INVALID_HANDLE_VALUE;
#endif

#ifndef LEAN_CLIENT
k5_mutex_t gssint_krb5_keytab_lock = K5_MUTEX_PARTIAL_INITIALIZER;
static char *krb5_gss_keytab = NULL;

/* Heimdal calls this gsskrb5_register_acceptor_identity. */
OM_uint32
gss_krb5int_register_acceptor_identity(OM_uint32 *minor_status,
                                       const gss_OID desired_mech,
                                       const gss_OID desired_object,
                                       gss_buffer_t value)
{
    char *new = NULL, *old;
    int err;

    err = gss_krb5int_initialize_library();
    if (err != 0)
        return GSS_S_FAILURE;

    if (value->value != NULL) {
        new = strdup((char *)value->value);
        if (new == NULL)
            return GSS_S_FAILURE;
    }

    k5_mutex_lock(&gssint_krb5_keytab_lock);
    old = krb5_gss_keytab;
    krb5_gss_keytab = new;
    k5_mutex_unlock(&gssint_krb5_keytab_lock);
    free(old);
    return GSS_S_COMPLETE;
}

/* Try to verify that keytab contains at least one entry for name.  Return 0 if
 * it does, KRB5_KT_NOTFOUND if it doesn't, or another error as appropriate. */
static krb5_error_code
check_keytab(krb5_context context, krb5_keytab kt, krb5_gss_name_t name)
{
    krb5_error_code code;
    krb5_keytab_entry ent;
    krb5_kt_cursor cursor;
    krb5_principal accprinc = NULL;
    krb5_boolean match;
    char *princname;

    if (name->service == NULL) {
        code = krb5_kt_get_entry(context, kt, name->princ, 0, 0, &ent);
        if (code == 0)
            krb5_kt_free_entry(context, &ent);
        return code;
    }

    /* If we can't iterate through the keytab, skip this check. */
    if (kt->ops->start_seq_get == NULL)
        return 0;

    /* Get the partial principal for the acceptor name. */
    code = kg_acceptor_princ(context, name, &accprinc);
    if (code)
        return code;

    /* Scan the keytab for host-based entries matching accprinc. */
    code = krb5_kt_start_seq_get(context, kt, &cursor);
    if (code)
        goto cleanup;
    while ((code = krb5_kt_next_entry(context, kt, &ent, &cursor)) == 0) {
        match = krb5_sname_match(context, accprinc, ent.principal);
        (void)krb5_free_keytab_entry_contents(context, &ent);
        if (match)
            break;
    }
    (void)krb5_kt_end_seq_get(context, kt, &cursor);
    if (code == KRB5_KT_END) {
        code = KRB5_KT_NOTFOUND;
        if (krb5_unparse_name(context, accprinc, &princname) == 0) {
            k5_setmsg(context, code, _("No key table entry found matching %s"),
                      princname);
            free(princname);
        }
    }

cleanup:
    krb5_free_principal(context, accprinc);
    return code;
}

/* get credentials corresponding to a key in the krb5 keytab.
   If successful, set the keytab-specific fields in cred
*/

static OM_uint32
acquire_accept_cred(krb5_context context, OM_uint32 *minor_status,
                    krb5_keytab req_keytab, const char *rcname,
                    krb5_gss_cred_id_rec *cred)
{
    OM_uint32 major;
    krb5_error_code code;
    krb5_keytab kt = NULL;
    krb5_rcache rc = NULL;

    assert(cred->keytab == NULL);

    /* If we have an explicit rcache name, open it. */
    if (rcname != NULL) {
        code = krb5_rc_resolve_full(context, &rc, rcname);
        if (code) {
            major = GSS_S_FAILURE;
            goto cleanup;
        }
        code = krb5_rc_recover_or_initialize(context, rc, context->clockskew);
        if (code) {
            major = GSS_S_FAILURE;
            goto cleanup;
        }
    }

    if (req_keytab != NULL) {
        code = krb5_kt_dup(context, req_keytab, &kt);
    } else {
        k5_mutex_lock(&gssint_krb5_keytab_lock);
        if (krb5_gss_keytab != NULL) {
            code = krb5_kt_resolve(context, krb5_gss_keytab, &kt);
            k5_mutex_unlock(&gssint_krb5_keytab_lock);
        } else {
            k5_mutex_unlock(&gssint_krb5_keytab_lock);
            code = krb5_kt_default(context, &kt);
        }
    }
    if (code) {
        major = GSS_S_CRED_UNAVAIL;
        goto cleanup;
    }

    if (cred->name != NULL) {
        /* Make sure we have keys matching the desired name in the keytab. */
        code = check_keytab(context, kt, cred->name);
        if (code) {
            if (code == KRB5_KT_NOTFOUND) {
                k5_change_error_message_code(context, code, KG_KEYTAB_NOMATCH);
                code = KG_KEYTAB_NOMATCH;
            }
            major = GSS_S_CRED_UNAVAIL;
            goto cleanup;
        }

        if (rc == NULL) {
            /* Open the replay cache for this principal. */
            code = krb5_get_server_rcache(context, &cred->name->princ->data[0],
                                          &rc);
            if (code) {
                major = GSS_S_FAILURE;
                goto cleanup;
            }
        }
    } else {
        /* Make sure we have a keytab with keys in it. */
        code = krb5_kt_have_content(context, kt);
        if (code) {
            major = GSS_S_CRED_UNAVAIL;
            goto cleanup;
        }
    }

    cred->keytab = kt;
    kt = NULL;
    cred->rcache = rc;
    rc = NULL;
    major = GSS_S_COMPLETE;

cleanup:
    if (kt != NULL)
        krb5_kt_close(context, kt);
    if (rc != NULL)
        krb5_rc_close(context, rc);
    *minor_status = code;
    return major;
}
#endif /* LEAN_CLIENT */

#ifdef USE_LEASH
static krb5_error_code
get_ccache_leash(krb5_context context, krb5_principal desired_princ,
                 krb5_ccache *ccache_out)
{
    krb5_error_code code;
    krb5_ccache ccache;
    char ccname[256] = "";

    *ccache_out = NULL;

    if (hLeashDLL == INVALID_HANDLE_VALUE) {
        hLeashDLL = LoadLibrary(LEASH_DLL);
        if (hLeashDLL != INVALID_HANDLE_VALUE) {
            (FARPROC) pLeash_AcquireInitialTicketsIfNeeded =
                GetProcAddress(hLeashDLL, "not_an_API_Leash_AcquireInitialTicketsIfNeeded");
        }
    }

    if (pLeash_AcquireInitialTicketsIfNeeded) {
        pLeash_AcquireInitialTicketsIfNeeded(context, desired_princ, ccname,
                                             sizeof(ccname));
        if (!ccname[0])
            return KRB5_CC_NOTFOUND;

        code = krb5_cc_resolve(context, ccname, &ccache);
        if (code)
            return code;
    } else {
        /* leash dll not available, open the default credential cache. */
        code = krb5int_cc_default(context, &ccache);
        if (code)
            return code;
    }

    *ccache_out = ccache;
    return 0;
}
#endif /* USE_LEASH */

/* Set fields in cred according to a ccache config entry whose key (in
 * principal form) is config_princ and whose value is value. */
static krb5_error_code
scan_cc_config(krb5_context context, krb5_gss_cred_id_rec *cred,
               krb5_const_principal config_princ, const krb5_data *value)
{
    krb5_error_code code;
    krb5_data data0 = empty_data();

    if (config_princ->length != 2)
        return 0;
    if (data_eq_string(config_princ->data[1], KRB5_CC_CONF_PROXY_IMPERSONATOR)
        && cred->impersonator == NULL) {
        code = krb5int_copy_data_contents_add0(context, value, &data0);
        if (code)
            return code;
        code = krb5_parse_name(context, data0.data, &cred->impersonator);
        krb5_free_data_contents(context, &data0);
        if (code)
            return code;
    } else if (data_eq_string(config_princ->data[1], KRB5_CC_CONF_REFRESH_TIME)
               && cred->refresh_time == 0) {
        code = krb5int_copy_data_contents_add0(context, value, &data0);
        if (code)
            return code;
        cred->refresh_time = atol(data0.data);
        krb5_free_data_contents(context, &data0);
    }
    return 0;
}

/* Return true if it appears that we can non-interactively get initial
 * tickets for cred. */
static krb5_boolean
can_get_initial_creds(krb5_context context, krb5_gss_cred_id_rec *cred)
{
    krb5_error_code code;
    krb5_keytab_entry entry;

    if (cred->password != NULL)
        return TRUE;

    if (cred->client_keytab == NULL)
        return FALSE;

    /* If we don't know the client principal yet, check for any keytab keys. */
    if (cred->name == NULL)
        return !krb5_kt_have_content(context, cred->client_keytab);

    /* Check if we have a keytab key for the client principal. */
    code = krb5_kt_get_entry(context, cred->client_keytab, cred->name->princ,
                             0, 0, &entry);
    if (code) {
        krb5_clear_error_message(context);
        return FALSE;
    }
    krb5_free_keytab_entry_contents(context, &entry);
    return TRUE;
}

/* Scan cred->ccache for name, expiry time, impersonator, refresh time. */
static krb5_error_code
scan_ccache(krb5_context context, krb5_gss_cred_id_rec *cred)
{
    krb5_error_code code;
    krb5_ccache ccache = cred->ccache;
    krb5_principal ccache_princ = NULL, tgt_princ = NULL;
    krb5_data *realm;
    krb5_cc_cursor cursor;
    krb5_creds creds;
    krb5_timestamp endtime;
    krb5_boolean is_tgt;

    /* Turn on NOTICKET, as we don't need session keys here. */
    code = krb5_cc_set_flags(context, ccache, KRB5_TC_NOTICKET);
    if (code)
        return code;

    /* Credentials cache principal must match the initiator name. */
    code = krb5_cc_get_principal(context, ccache, &ccache_princ);
    if (code != 0)
        goto cleanup;
    if (cred->name != NULL &&
        !krb5_principal_compare(context, ccache_princ, cred->name->princ)) {
        code = KG_CCACHE_NOMATCH;
        goto cleanup;
    }

    /* Save the ccache principal as the credential name if not already set. */
    if (!cred->name) {
        code = kg_init_name(context, ccache_princ, NULL, NULL, NULL,
                            KG_INIT_NAME_NO_COPY, &cred->name);
        if (code)
            goto cleanup;
        ccache_princ = NULL;
    }

    assert(cred->name->princ != NULL);
    realm = krb5_princ_realm(context, cred->name->princ);
    code = krb5_build_principal_ext(context, &tgt_princ,
                                    realm->length, realm->data,
                                    KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME,
                                    realm->length, realm->data,
                                    0);
    if (code)
        return code;

    /* If there's a tgt for the principal's local realm in here, use its expiry
     * time.  Otherwise use the first key. */
    code = krb5_cc_start_seq_get(context, ccache, &cursor);
    if (code) {
        krb5_free_principal(context, tgt_princ);
        return code;
    }
    while (!(code = krb5_cc_next_cred(context, ccache, &cursor, &creds))) {
        if (krb5_is_config_principal(context, creds.server)) {
            code = scan_cc_config(context, cred, creds.server, &creds.ticket);
            krb5_free_cred_contents(context, &creds);
            if (code)
                break;
            continue;
        }
        is_tgt = krb5_principal_compare(context, tgt_princ, creds.server);
        endtime = creds.times.endtime;
        krb5_free_cred_contents(context, &creds);
        if (is_tgt)
            cred->have_tgt = TRUE;
        if (is_tgt || cred->expire == 0)
            cred->expire = endtime;
    }
    krb5_cc_end_seq_get(context, ccache, &cursor);
    if (code && code != KRB5_CC_END)
        goto cleanup;
    code = 0;

    if (cred->expire == 0 && !can_get_initial_creds(context, cred)) {
        code = KG_EMPTY_CCACHE;
        goto cleanup;
    }

cleanup:
    (void)krb5_cc_set_flags(context, ccache, 0);
    krb5_free_principal(context, ccache_princ);
    krb5_free_principal(context, tgt_princ);
    return code;
}

/* Find an existing or destination ccache for cred->name. */
static krb5_error_code
get_cache_for_name(krb5_context context, krb5_gss_cred_id_rec *cred)
{
    krb5_error_code code;
    krb5_boolean can_get, have_collection;
    krb5_ccache defcc = NULL;
    krb5_principal princ = NULL;
    const char *cctype;

    assert(cred->name != NULL && cred->ccache == NULL);
#ifdef USE_LEASH
    code = get_ccache_leash(context, cred->name->princ, &cred->ccache);
    return code ? code : scan_ccache(context, cred);
#else
    /* Check first whether we can acquire tickets, to avoid overwriting the
     * extended error message from krb5_cc_cache_match. */
    can_get = can_get_initial_creds(context, cred);

    /* Look for an existing cache for the client principal. */
    code = krb5_cc_cache_match(context, cred->name->princ, &cred->ccache);
    if (code == 0)
        return scan_ccache(context, cred);
    if (code != KRB5_CC_NOTFOUND || !can_get)
        return code;
    krb5_clear_error_message(context);

    /* There is no existing ccache, but we can acquire credentials.  Get the
     * default ccache to help decide where we should put them. */
    code = krb5_cc_default(context, &defcc);
    if (code)
        return code;
    cctype = krb5_cc_get_type(context, defcc);
    have_collection = krb5_cc_support_switch(context, cctype);

    /* We can use an empty default ccache if we're using a password or if
     * there's no collection. */
    if (cred->password != NULL || !have_collection) {
        if (krb5_cc_get_principal(context, defcc, &princ) == KRB5_FCC_NOFILE) {
            cred->ccache = defcc;
            defcc = NULL;
        }
        krb5_clear_error_message(context);
    }

    /* Otherwise, try to use a new cache in the collection. */
    if (cred->ccache == NULL) {
        if (!have_collection) {
            code = KG_CCACHE_NOMATCH;
            goto cleanup;
        }
        code = krb5_cc_new_unique(context, cctype, NULL, &cred->ccache);
        if (code)
            goto cleanup;
    }

cleanup:
    krb5_free_principal(context, princ);
    if (defcc != NULL)
        krb5_cc_close(context, defcc);
    return code;
#endif /* not USE_LEASH */
}

/* Try to set cred->name using the client keytab. */
static krb5_error_code
get_name_from_client_keytab(krb5_context context, krb5_gss_cred_id_rec *cred)
{
    krb5_error_code code;
    krb5_principal princ;

    assert(cred->name == NULL);

    if (cred->client_keytab == NULL)
        return KRB5_KT_NOTFOUND;

    code = k5_kt_get_principal(context, cred->client_keytab, &princ);
    if (code)
        return code;
    code = kg_init_name(context, princ, NULL, NULL, NULL, KG_INIT_NAME_NO_COPY,
                        &cred->name);
    if (code) {
        krb5_free_principal(context, princ);
        return code;
    }
    return 0;
}

/* Make a note in ccache that we should attempt to refresh it from the client
 * keytab at refresh_time. */
static void
set_refresh_time(krb5_context context, krb5_ccache ccache,
                 krb5_timestamp refresh_time)
{
    char buf[128];
    krb5_data d;

    snprintf(buf, sizeof(buf), "%u", (unsigned int)ts2tt(refresh_time));
    d = string2data(buf);
    (void)krb5_cc_set_config(context, ccache, NULL, KRB5_CC_CONF_REFRESH_TIME,
                             &d);
    krb5_clear_error_message(context);
}

/* Return true if it's time to refresh cred from the client keytab.  If
 * returning true, avoid retrying for 30 seconds. */
krb5_boolean
kg_cred_time_to_refresh(krb5_context context, krb5_gss_cred_id_rec *cred)
{
    krb5_timestamp now;

    if (krb5_timeofday(context, &now))
        return FALSE;
    if (cred->refresh_time != 0 && !ts_after(cred->refresh_time, now)) {
        set_refresh_time(context, cred->ccache,
                         ts_incr(cred->refresh_time, 30));
        return TRUE;
    }
    return FALSE;
}

/* If appropriate, make a note to refresh cred from the client keytab when it
 * is halfway to expired. */
void
kg_cred_set_initial_refresh(krb5_context context, krb5_gss_cred_id_rec *cred,
                            krb5_ticket_times *times)
{
    krb5_timestamp refresh;

    /* For now, we only mark keytab-acquired credentials for refresh. */
    if (cred->password != NULL)
        return;

    /* Make a note to refresh these when they are halfway to expired. */
    refresh = ts_incr(times->starttime,
                      ts_delta(times->endtime, times->starttime) / 2);
    set_refresh_time(context, cred->ccache, refresh);
}

/* Get initial credentials using the supplied password or client keytab. */
static krb5_error_code
get_initial_cred(krb5_context context, krb5_gss_cred_id_rec *cred)
{
    krb5_error_code code;
    krb5_get_init_creds_opt *opt = NULL;
    krb5_creds creds;

    code = krb5_get_init_creds_opt_alloc(context, &opt);
    if (code)
        return code;
    code = krb5_get_init_creds_opt_set_out_ccache(context, opt, cred->ccache);
    if (code)
        goto cleanup;
    if (cred->password != NULL) {
        code = krb5_get_init_creds_password(context, &creds, cred->name->princ,
                                            cred->password, NULL, NULL, 0,
                                            NULL, opt);
    } else if (cred->client_keytab != NULL) {
        code = krb5_get_init_creds_keytab(context, &creds, cred->name->princ,
                                          cred->client_keytab, 0, NULL, opt);
    } else {
        code = KRB5_KT_NOTFOUND;
    }
    if (code)
        goto cleanup;
    kg_cred_set_initial_refresh(context, cred, &creds.times);
    cred->have_tgt = TRUE;
    cred->expire = creds.times.endtime;
    krb5_free_cred_contents(context, &creds);
cleanup:
    krb5_get_init_creds_opt_free(context, opt);
    return code;
}

/* Get initial credentials if we ought to and are able to. */
static krb5_error_code
maybe_get_initial_cred(krb5_context context, krb5_gss_cred_id_rec *cred)
{
    krb5_error_code code;

    /* Don't get creds if we don't know the name or are doing IAKERB. */
    if (cred->name == NULL || cred->iakerb_mech)
        return 0;

    /* Get creds if we have none or if it's time to refresh. */
    if (cred->expire == 0 || kg_cred_time_to_refresh(context, cred)) {
        code = get_initial_cred(context, cred);
        /* If we were trying to refresh and failed, we can keep going. */
        if (code && cred->expire == 0)
            return code;
        krb5_clear_error_message(context);
    }
    return 0;
}

static OM_uint32
acquire_init_cred(krb5_context context,
                  OM_uint32 *minor_status,
                  krb5_ccache req_ccache,
                  gss_buffer_t password,
                  krb5_keytab client_keytab,
                  krb5_gss_cred_id_rec *cred)
{
    krb5_error_code code;
    krb5_data pwdata, pwcopy;
    int caller_ccname = 0;

    /* Get ccache from caller if available. */
    if (GSS_ERROR(kg_sync_ccache_name(context, minor_status)))
        return GSS_S_FAILURE;
    if (GSS_ERROR(kg_caller_provided_ccache_name(minor_status,
                                                 &caller_ccname)))
        return GSS_S_FAILURE;

    if (password != GSS_C_NO_BUFFER) {
        pwdata = make_data(password->value, password->length);
        code = krb5int_copy_data_contents_add0(context, &pwdata, &pwcopy);
        if (code)
            goto error;
        cred->password = pwcopy.data;

        /* We will fetch the credential into a private memory ccache. */
        assert(req_ccache == NULL);
        code = krb5_cc_new_unique(context, "MEMORY", NULL, &cred->ccache);
        if (code)
            goto error;
        cred->destroy_ccache = 1;
    } else if (req_ccache != NULL) {
        code = krb5_cc_dup(context, req_ccache, &cred->ccache);
        if (code)
            goto error;
    } else if (caller_ccname) {
        /* Caller's ccache name has been set as the context default. */
        code = krb5int_cc_default(context, &cred->ccache);
        if (code)
            goto error;
    }

    if (client_keytab != NULL) {
        code = krb5_kt_dup(context, client_keytab, &cred->client_keytab);
    } else {
        code = krb5_kt_client_default(context, &cred->client_keytab);
        if (code) {
            /* Treat resolution failure similarly to a client keytab which
             * resolves but doesn't exist or has no content. */
            TRACE_GSS_CLIENT_KEYTAB_FAIL(context, code);
            krb5_clear_error_message(context);
            code = 0;
        }
    }
    if (code)
        goto error;

    if (cred->ccache != NULL) {
        /* The caller specified a ccache; check what's in it. */
        code = scan_ccache(context, cred);
        if (code == KRB5_FCC_NOFILE) {
            /* See if we can get initial creds.  If the caller didn't specify
             * a name, pick one from the client keytab. */
            if (cred->name == NULL) {
                if (!get_name_from_client_keytab(context, cred))
                    code = 0;
            } else if (can_get_initial_creds(context, cred)) {
                code = 0;
            }
        }
        if (code)
            goto error;
    } else if (cred->name != NULL) {
        /* The caller specified a name but not a ccache; pick a cache. */
        code = get_cache_for_name(context, cred);
        if (code)
            goto error;
    }

#ifndef USE_LEASH
    /* If we haven't picked a name, make sure we have or can get any creds,
     * unless we're using Leash and might be able to get them interactively. */
    if (cred->name == NULL && !can_get_initial_creds(context, cred)) {
        code = krb5_cccol_have_content(context);
        if (code)
            goto error;
    }
#endif

    code = maybe_get_initial_cred(context, cred);
    if (code)
        goto error;

    *minor_status = 0;
    return GSS_S_COMPLETE;

error:
    *minor_status = code;
    return GSS_S_CRED_UNAVAIL;
}

static OM_uint32
acquire_cred_context(krb5_context context, OM_uint32 *minor_status,
                     gss_name_t desired_name, gss_buffer_t password,
                     OM_uint32 time_req, gss_cred_usage_t cred_usage,
                     krb5_ccache ccache, krb5_keytab client_keytab,
                     krb5_keytab keytab, const char *rcname,
                     krb5_boolean iakerb, gss_cred_id_t *output_cred_handle,
                     OM_uint32 *time_rec)
{
    krb5_gss_cred_id_t cred = NULL;
    krb5_gss_name_t name = (krb5_gss_name_t)desired_name;
    OM_uint32 ret;
    krb5_error_code code = 0;

    /* make sure all outputs are valid */
    *output_cred_handle = GSS_C_NO_CREDENTIAL;
    if (time_rec)
        *time_rec = 0;

    /* create the gss cred structure */
    cred = k5alloc(sizeof(krb5_gss_cred_id_rec), &code);
    if (cred == NULL)
        goto krb_error_out;

    cred->usage = cred_usage;
    cred->name = NULL;
    cred->impersonator = NULL;
    cred->iakerb_mech = iakerb;
    cred->default_identity = (name == NULL);
#ifndef LEAN_CLIENT
    cred->keytab = NULL;
#endif /* LEAN_CLIENT */
    cred->destroy_ccache = 0;
    cred->suppress_ci_flags = 0;
    cred->ccache = NULL;

    code = k5_mutex_init(&cred->lock);
    if (code)
        goto krb_error_out;

    switch (cred_usage) {
    case GSS_C_INITIATE:
    case GSS_C_ACCEPT:
    case GSS_C_BOTH:
        break;
    default:
        ret = GSS_S_FAILURE;
        *minor_status = (OM_uint32) G_BAD_USAGE;
        goto error_out;
    }

    if (name != NULL) {
        code = kg_duplicate_name(context, name, &cred->name);
        if (code)
            goto krb_error_out;
    }

#ifndef LEAN_CLIENT
    /*
     * If requested, acquire credentials for accepting. This will fill
     * in cred->name if desired_princ is specified.
     */
    if (cred_usage == GSS_C_ACCEPT || cred_usage == GSS_C_BOTH) {
        ret = acquire_accept_cred(context, minor_status, keytab, rcname, cred);
        if (ret != GSS_S_COMPLETE)
            goto error_out;
    }
#endif /* LEAN_CLIENT */

    /*
     * If requested, acquire credentials for initiation. This will fill
     * in cred->name if it wasn't set above.
     */
    if (cred_usage == GSS_C_INITIATE || cred_usage == GSS_C_BOTH) {
        ret = acquire_init_cred(context, minor_status, ccache, password,
                                client_keytab, cred);
        if (ret != GSS_S_COMPLETE)
            goto error_out;
    }

    assert(cred->default_identity || cred->name != NULL);

    /*** at this point, the cred structure has been completely created */

    if (cred_usage == GSS_C_ACCEPT) {
        if (time_rec)
            *time_rec = GSS_C_INDEFINITE;
    } else {
        krb5_timestamp now;

        code = krb5_timeofday(context, &now);
        if (code != 0)
            goto krb_error_out;

        if (time_rec) {
            /* Resolve cred now to determine the expiration time. */
            ret = kg_cred_resolve(minor_status, context, (gss_cred_id_t)cred,
                                  GSS_C_NO_NAME);
            if (GSS_ERROR(ret))
                goto error_out;
            *time_rec = ts_after(cred->expire, now) ?
                ts_delta(cred->expire, now) : 0;
            k5_mutex_unlock(&cred->lock);
        }
    }

    *minor_status = 0;
    *output_cred_handle = (gss_cred_id_t) cred;

    return GSS_S_COMPLETE;

krb_error_out:
    *minor_status = code;
    ret = GSS_S_FAILURE;

error_out:
    if (cred != NULL) {
        if (cred->ccache) {
            if (cred->destroy_ccache)
                krb5_cc_destroy(context, cred->ccache);
            else
                krb5_cc_close(context, cred->ccache);
        }
        if (cred->client_keytab)
            krb5_kt_close(context, cred->client_keytab);
#ifndef LEAN_CLIENT
        if (cred->keytab)
            krb5_kt_close(context, cred->keytab);
#endif /* LEAN_CLIENT */
        if (cred->rcache)
            krb5_rc_close(context, cred->rcache);
        if (cred->name)
            kg_release_name(context, &cred->name);
        krb5_free_principal(context, cred->impersonator);
        zapfreestr(cred->password);
        k5_mutex_destroy(&cred->lock);
        xfree(cred);
    }
    save_error_info(*minor_status, context);
    return ret;
}

static OM_uint32
acquire_cred(OM_uint32 *minor_status, gss_name_t desired_name,
             gss_buffer_t password, OM_uint32 time_req,
             gss_cred_usage_t cred_usage, krb5_ccache ccache,
             krb5_keytab keytab, krb5_boolean iakerb,
             gss_cred_id_t *output_cred_handle, OM_uint32 *time_rec)
{
    krb5_context context = NULL;
    krb5_error_code code = 0;
    OM_uint32 ret;

    code = gss_krb5int_initialize_library();
    if (code) {
        *minor_status = code;
        ret = GSS_S_FAILURE;
        goto out;
    }

    code = krb5_gss_init_context(&context);
    if (code) {
        *minor_status = code;
        ret = GSS_S_FAILURE;
        goto out;
    }

    ret = acquire_cred_context(context, minor_status, desired_name, password,
                               time_req, cred_usage, ccache, NULL, keytab,
                               NULL, iakerb, output_cred_handle, time_rec);

out:
    krb5_free_context(context);
    return ret;
}

/*
 * Resolve the name and ccache for an initiator credential if it has not yet
 * been done.  If specified, use the target name to pick an appropriate ccache
 * within the collection.  Validates cred_handle and leaves it locked on
 * success.
 */
OM_uint32
kg_cred_resolve(OM_uint32 *minor_status, krb5_context context,
                gss_cred_id_t cred_handle, gss_name_t target_name)
{
    OM_uint32 maj;
    krb5_error_code code;
    krb5_gss_cred_id_t cred = (krb5_gss_cred_id_t)cred_handle;
    krb5_gss_name_t tname = (krb5_gss_name_t)target_name;
    krb5_principal client_princ;

    *minor_status = 0;

    maj = krb5_gss_validate_cred_1(minor_status, cred_handle, context);
    if (maj != 0)
        return maj;
    k5_mutex_assert_locked(&cred->lock);

    if (cred->usage == GSS_C_ACCEPT || cred->name != NULL)
        return GSS_S_COMPLETE;
    /* acquire_init_cred should have set both name and ccache, or neither. */
    assert(cred->ccache == NULL);

    if (tname != NULL) {
        /* Use the target name to select an existing ccache or a principal. */
        code = krb5_cc_select(context, tname->princ, &cred->ccache,
                              &client_princ);
        if (code && code != KRB5_CC_NOTFOUND)
            goto kerr;
        if (client_princ != NULL) {
            code = kg_init_name(context, client_princ, NULL, NULL, NULL,
                                KG_INIT_NAME_NO_COPY, &cred->name);
            if (code) {
                krb5_free_principal(context, client_princ);
                goto kerr;
            }
        }
        if (cred->ccache != NULL) {
            code = scan_ccache(context, cred);
            if (code)
                goto kerr;
        }
    }

    /* If we still haven't picked a client principal, try using an existing
     * default ccache.  (On Windows, this may acquire initial creds.) */
    if (cred->name == NULL) {
        code = krb5int_cc_default(context, &cred->ccache);
        if (code)
            goto kerr;
        code = scan_ccache(context, cred);
        if (code == KRB5_FCC_NOFILE) {
            /* Default ccache doesn't exist; fall through to client keytab. */
            krb5_cc_close(context, cred->ccache);
            cred->ccache = NULL;
        } else if (code) {
            goto kerr;
        }
    }

    /* If that didn't work, try getting a name from the client keytab. */
    if (cred->name == NULL) {
        code = get_name_from_client_keytab(context, cred);
        if (code) {
            code = KG_EMPTY_CCACHE;
            goto kerr;
        }
    }

    if (cred->name != NULL && cred->ccache == NULL) {
        /* Pick a cache for the name we chose (from krb5_cc_select or from the
         * client keytab). */
        code = get_cache_for_name(context, cred);
        if (code)
            goto kerr;
    }

    /* Resolve name to ccache and possibly get initial credentials. */
    code = maybe_get_initial_cred(context, cred);
    if (code)
        goto kerr;

    return GSS_S_COMPLETE;

kerr:
    k5_mutex_unlock(&cred->lock);
    save_error_info(code, context);
    *minor_status = code;
    return GSS_S_CRED_UNAVAIL;
}

OM_uint32
gss_krb5int_set_cred_rcache(OM_uint32 *minor_status,
                            gss_cred_id_t *cred_handle,
                            const gss_OID desired_oid,
                            const gss_buffer_t value)
{
    krb5_gss_cred_id_t cred;
    krb5_error_code code;
    krb5_context context;
    krb5_rcache rcache;

    assert(value->length == sizeof(rcache));

    if (value->length != sizeof(rcache))
        return GSS_S_FAILURE;

    rcache = (krb5_rcache)value->value;

    cred = (krb5_gss_cred_id_t)*cred_handle;

    code = krb5_gss_init_context(&context);
    if (code) {
        *minor_status = code;
        return GSS_S_FAILURE;
    }
    if (cred->rcache != NULL) {
        code = krb5_rc_close(context, cred->rcache);
        if (code) {
            *minor_status = code;
            krb5_free_context(context);
            return GSS_S_FAILURE;
        }
    }

    cred->rcache = rcache;

    krb5_free_context(context);

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

/*
 * krb5 and IAKERB mech API functions follow.  The mechglue always passes null
 * desired_mechs and actual_mechs, so we ignore those parameters.
 */

OM_uint32 KRB5_CALLCONV
krb5_gss_acquire_cred(OM_uint32 *minor_status, gss_name_t desired_name,
                      OM_uint32 time_req, gss_OID_set desired_mechs,
                      gss_cred_usage_t cred_usage,
                      gss_cred_id_t *output_cred_handle,
                      gss_OID_set *actual_mechs, OM_uint32 *time_rec)
{
    return acquire_cred(minor_status, desired_name, NULL, time_req, cred_usage,
                        NULL, NULL, FALSE, output_cred_handle, time_rec);
}

OM_uint32 KRB5_CALLCONV
iakerb_gss_acquire_cred(OM_uint32 *minor_status, gss_name_t desired_name,
                        OM_uint32 time_req, gss_OID_set desired_mechs,
                        gss_cred_usage_t cred_usage,
                        gss_cred_id_t *output_cred_handle,
                        gss_OID_set *actual_mechs, OM_uint32 *time_rec)
{
    return acquire_cred(minor_status, desired_name, NULL, time_req, cred_usage,
                        NULL, NULL, TRUE, output_cred_handle, time_rec);
}

OM_uint32 KRB5_CALLCONV
krb5_gss_acquire_cred_with_password(OM_uint32 *minor_status,
                                    const gss_name_t desired_name,
                                    const gss_buffer_t password,
                                    OM_uint32 time_req,
                                    const gss_OID_set desired_mechs,
                                    int cred_usage,
                                    gss_cred_id_t *output_cred_handle,
                                    gss_OID_set *actual_mechs,
                                    OM_uint32 *time_rec)
{
    return acquire_cred(minor_status, desired_name, password, time_req,
                        cred_usage, NULL, NULL, FALSE, output_cred_handle,
                        time_rec);
}

OM_uint32 KRB5_CALLCONV
iakerb_gss_acquire_cred_with_password(OM_uint32 *minor_status,
                                      const gss_name_t desired_name,
                                      const gss_buffer_t password,
                                      OM_uint32 time_req,
                                      const gss_OID_set desired_mechs,
                                      int cred_usage,
                                      gss_cred_id_t *output_cred_handle,
                                      gss_OID_set *actual_mechs,
                                      OM_uint32 *time_rec)
{
    return acquire_cred(minor_status, desired_name, password, time_req,
                        cred_usage, NULL, NULL, TRUE, output_cred_handle,
                        time_rec);
}

OM_uint32
gss_krb5int_import_cred(OM_uint32 *minor_status,
                        gss_cred_id_t *cred_handle,
                        const gss_OID desired_oid,
                        const gss_buffer_t value)
{
    struct krb5_gss_import_cred_req *req;
    krb5_gss_name_rec name;
    OM_uint32 time_rec;
    krb5_error_code code;
    gss_cred_usage_t usage;
    gss_name_t desired_name = GSS_C_NO_NAME;

    assert(value->length == sizeof(*req));

    if (value->length != sizeof(*req))
        return GSS_S_FAILURE;

    req = (struct krb5_gss_import_cred_req *)value->value;

    if (req->id != NULL) {
        usage = (req->keytab != NULL) ? GSS_C_BOTH : GSS_C_INITIATE;
    } else if (req->keytab != NULL) {
        usage = GSS_C_ACCEPT;
    } else {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    if (req->keytab_principal != NULL) {
        memset(&name, 0, sizeof(name));
        code = k5_mutex_init(&name.lock);
        if (code != 0) {
            *minor_status = code;
            return GSS_S_FAILURE;
        }
        name.princ = req->keytab_principal;
        desired_name = (gss_name_t)&name;
    }

    code = acquire_cred(minor_status, desired_name, NULL, GSS_C_INDEFINITE,
                        usage, req->id, req->keytab, FALSE, cred_handle,
                        &time_rec);
    if (req->keytab_principal != NULL)
        k5_mutex_destroy(&name.lock);
    return code;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_acquire_cred_from(OM_uint32 *minor_status,
                           const gss_name_t desired_name,
                           OM_uint32 time_req,
                           const gss_OID_set desired_mechs,
                           gss_cred_usage_t cred_usage,
                           gss_const_key_value_set_t cred_store,
                           gss_cred_id_t *output_cred_handle,
                           gss_OID_set *actual_mechs,
                           OM_uint32 *time_rec)
{
    krb5_context context = NULL;
    krb5_error_code code = 0;
    krb5_keytab client_keytab = NULL;
    krb5_keytab keytab = NULL;
    krb5_ccache ccache = NULL;
    const char *rcname, *value;
    OM_uint32 ret;

    code = gss_krb5int_initialize_library();
    if (code) {
        *minor_status = code;
        ret = GSS_S_FAILURE;
        goto out;
    }

    code = krb5_gss_init_context(&context);
    if (code) {
        *minor_status = code;
        ret = GSS_S_FAILURE;
        goto out;
    }

    ret = kg_value_from_cred_store(cred_store, KRB5_CS_CCACHE_URN, &value);
    if (GSS_ERROR(ret))
        goto out;

    if (value) {
        code = krb5_cc_resolve(context, value, &ccache);
        if (code != 0) {
            *minor_status = code;
            ret = GSS_S_CRED_UNAVAIL;
            goto out;
        }
    }

    ret = kg_value_from_cred_store(cred_store, KRB5_CS_CLI_KEYTAB_URN, &value);
    if (GSS_ERROR(ret))
        goto out;

    if (value) {
        code = krb5_kt_resolve(context, value, &client_keytab);
        if (code != 0) {
            *minor_status = code;
            ret = GSS_S_CRED_UNAVAIL;
            goto out;
        }
    }

    ret = kg_value_from_cred_store(cred_store, KRB5_CS_KEYTAB_URN, &value);
    if (GSS_ERROR(ret))
        goto out;

    if (value) {
        code = krb5_kt_resolve(context, value, &keytab);
        if (code != 0) {
            *minor_status = code;
            ret = GSS_S_CRED_UNAVAIL;
            goto out;
        }
    }

    ret = kg_value_from_cred_store(cred_store, KRB5_CS_RCACHE_URN, &rcname);
    if (GSS_ERROR(ret))
        goto out;

    ret = acquire_cred_context(context, minor_status, desired_name, NULL,
                               time_req, cred_usage, ccache, client_keytab,
                               keytab, rcname, 0, output_cred_handle,
                               time_rec);

out:
    if (ccache != NULL)
        krb5_cc_close(context, ccache);
    if (client_keytab != NULL)
        krb5_kt_close(context, client_keytab);
    if (keytab != NULL)
        krb5_kt_close(context, keytab);
    krb5_free_context(context);
    return ret;
}
