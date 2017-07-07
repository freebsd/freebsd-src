/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/gic_keytab.c */
/*
 * Copyright (C) 2002, 2003, 2008 by the Massachusetts Institute of Technology.
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
#ifndef LEAN_CLIENT

#include "k5-int.h"
#include "int-proto.h"
#include "init_creds_ctx.h"

static krb5_error_code
get_as_key_keytab(krb5_context context,
                  krb5_principal client,
                  krb5_enctype etype,
                  krb5_prompter_fct prompter,
                  void *prompter_data,
                  krb5_data *salt,
                  krb5_data *params,
                  krb5_keyblock *as_key,
                  void *gak_data,
                  k5_response_items *ritems)
{
    krb5_keytab keytab = (krb5_keytab) gak_data;
    krb5_error_code ret;
    krb5_keytab_entry kt_ent;
    krb5_keyblock *kt_key;

    /* We don't need the password from the responder to create the AS key. */
    if (as_key == NULL)
        return 0;

    /* if there's already a key of the correct etype, we're done.
       if the etype is wrong, free the existing key, and make
       a new one. */

    if (as_key->length) {
        if (as_key->enctype == etype)
            return(0);

        krb5_free_keyblock_contents(context, as_key);
        as_key->length = 0;
    }

    if (!krb5_c_valid_enctype(etype))
        return(KRB5_PROG_ETYPE_NOSUPP);

    if ((ret = krb5_kt_get_entry(context, keytab, client,
                                 0, /* don't have vno available */
                                 etype, &kt_ent)))
        return(ret);

    ret = krb5_copy_keyblock(context, &kt_ent.key, &kt_key);

    /* again, krb5's memory management is lame... */

    *as_key = *kt_key;
    free(kt_key);

    (void) krb5_kt_free_entry(context, &kt_ent);

    return(ret);
}

/* Return the list of etypes available for client in keytab. */
static krb5_error_code
lookup_etypes_for_keytab(krb5_context context, krb5_keytab keytab,
                         krb5_principal client, krb5_enctype **etypes_out)
{
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;
    krb5_enctype *p, *etypes = NULL, etype;
    krb5_kvno max_kvno = 0, vno;
    krb5_error_code ret;
    krb5_boolean match;
    size_t count = 0;

    *etypes_out = NULL;

    if (keytab->ops->start_seq_get == NULL)
        return EINVAL;
    ret = krb5_kt_start_seq_get(context, keytab, &cursor);
    if (ret != 0)
        return ret;

    while (!(ret = krb5_kt_next_entry(context, keytab, &entry, &cursor))) {
        /* Extract what we need from the entry and free it. */
        etype = entry.key.enctype;
        vno = entry.vno;
        match = krb5_principal_compare(context, entry.principal, client);
        krb5_free_keytab_entry_contents(context, &entry);

        /* Filter out old or non-matching entries and invalid enctypes. */
        if (vno < max_kvno || !match || !krb5_c_valid_enctype(etype))
            continue;

        /* Update max_kvno and reset the list if we find a newer kvno. */
        if (vno > max_kvno) {
            max_kvno = vno;
            free(etypes);
            etypes = NULL;
            count = 0;
        }

        /* Leave room for the terminator and possibly a second entry. */
        p = realloc(etypes, (count + 3) * sizeof(*etypes));
        if (p == NULL) {
            ret = ENOMEM;
            goto cleanup;
        }
        etypes = p;
        etypes[count++] = etype;
        /* All DES key types work with des-cbc-crc, which is more likely to be
         * accepted by the KDC (since MIT KDCs refuse des-cbc-md5). */
        if (etype == ENCTYPE_DES_CBC_MD5 || etype == ENCTYPE_DES_CBC_MD4)
            etypes[count++] = ENCTYPE_DES_CBC_CRC;
        etypes[count] = 0;
    }
    if (ret != KRB5_KT_END)
        goto cleanup;
    ret = 0;

    *etypes_out = etypes;
    etypes = NULL;

cleanup:
    krb5_kt_end_seq_get(context, keytab, &cursor);
    free(etypes);
    return ret;
}

/* Move the entries in keytab_list (zero-terminated) to the front of req_list
 * (of length req_len), preserving order otherwise. */
static krb5_error_code
sort_enctypes(krb5_enctype *req_list, int req_len, krb5_enctype *keytab_list)
{
    krb5_enctype *save_list;
    int save_pos, req_pos, i;

    save_list = malloc(req_len * sizeof(*save_list));
    if (save_list == NULL)
        return ENOMEM;

    /* Sort req_list entries into the front of req_list or into save_list. */
    req_pos = save_pos = 0;
    for (i = 0; i < req_len; i++) {
        if (k5_etypes_contains(keytab_list, req_list[i]))
            req_list[req_pos++] = req_list[i];
        else
            save_list[save_pos++] = req_list[i];
    }

    /* Put the entries we saved back in at the end, in order. */
    for (i = 0; i < save_pos; i++)
        req_list[req_pos++] = save_list[i];
    assert(req_pos == req_len);

    free(save_list);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_init_creds_set_keytab(krb5_context context,
                           krb5_init_creds_context ctx,
                           krb5_keytab keytab)
{
    krb5_enctype *etype_list;
    krb5_error_code ret;
    char *name;

    ctx->gak_fct = get_as_key_keytab;
    ctx->gak_data = keytab;

    ret = lookup_etypes_for_keytab(context, keytab, ctx->request->client,
                                   &etype_list);
    if (ret) {
        TRACE_INIT_CREDS_KEYTAB_LOOKUP_FAILED(context, ret);
        return 0;
    }
    TRACE_INIT_CREDS_KEYTAB_LOOKUP(context, etype_list);

    /* Error out if we have no keys for the client principal. */
    if (etype_list == NULL) {
        ret = krb5_unparse_name(context, ctx->request->client, &name);
        if (ret == 0) {
            k5_setmsg(context, KRB5_KT_NOTFOUND,
                      _("Keytab contains no suitable keys for %s"), name);
        }
        krb5_free_unparsed_name(context, name);
        return KRB5_KT_NOTFOUND;
    }

    /* Sort the request enctypes so the ones in the keytab appear first. */
    ret = sort_enctypes(ctx->request->ktype, ctx->request->nktypes,
                        etype_list);
    free(etype_list);
    return ret;
}

static krb5_error_code
get_init_creds_keytab(krb5_context context, krb5_creds *creds,
                      krb5_principal client, krb5_keytab keytab,
                      krb5_deltat start_time, const char *in_tkt_service,
                      krb5_get_init_creds_opt *options, int *use_master)
{
    krb5_error_code ret;
    krb5_init_creds_context ctx = NULL;

    ret = krb5_init_creds_init(context, client, NULL, NULL, start_time,
                               options, &ctx);
    if (ret != 0)
        goto cleanup;

    if (in_tkt_service) {
        ret = krb5_init_creds_set_service(context, ctx, in_tkt_service);
        if (ret != 0)
            goto cleanup;
    }

    ret = krb5_init_creds_set_keytab(context, ctx, keytab);
    if (ret != 0)
        goto cleanup;

    ret = k5_init_creds_get(context, ctx, use_master);
    if (ret != 0)
        goto cleanup;

    ret = krb5_init_creds_get_creds(context, ctx, creds);
    if (ret != 0)
        goto cleanup;

cleanup:
    krb5_init_creds_free(context, ctx);

    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_keytab(krb5_context context,
                           krb5_creds *creds,
                           krb5_principal client,
                           krb5_keytab arg_keytab,
                           krb5_deltat start_time,
                           const char *in_tkt_service,
                           krb5_get_init_creds_opt *options)
{
    krb5_error_code ret;
    int use_master;
    krb5_keytab keytab;
    struct errinfo errsave = EMPTY_ERRINFO;

    if (arg_keytab == NULL) {
        if ((ret = krb5_kt_default(context, &keytab)))
            return ret;
    } else {
        keytab = arg_keytab;
    }

    use_master = 0;

    /* first try: get the requested tkt from any kdc */

    ret = get_init_creds_keytab(context, creds, client, keytab, start_time,
                                in_tkt_service, options, &use_master);

    /* check for success */

    if (ret == 0)
        goto cleanup;

    /* If all the kdc's are unavailable fail */

    if ((ret == KRB5_KDC_UNREACH) || (ret == KRB5_REALM_CANT_RESOLVE))
        goto cleanup;

    /* if the reply did not come from the master kdc, try again with
       the master kdc */

    if (!use_master) {
        use_master = 1;

        k5_save_ctx_error(context, ret, &errsave);
        ret = get_init_creds_keytab(context, creds, client, keytab,
                                    start_time, in_tkt_service, options,
                                    &use_master);
        if (ret == 0)
            goto cleanup;

        /* If the master is unreachable, return the error from the slave we
         * were able to contact. */
        if (ret == KRB5_KDC_UNREACH || ret == KRB5_REALM_CANT_RESOLVE ||
            ret == KRB5_REALM_UNKNOWN)
            ret = k5_restore_ctx_error(context, &errsave);
    }

    /* at this point, we have a response from the master.  Since we don't
       do any prompting or changing for keytabs, that's it. */

cleanup:
    if (arg_keytab == NULL)
        krb5_kt_close(context, keytab);
    k5_clear_error(&errsave);

    return(ret);
}
krb5_error_code KRB5_CALLCONV
krb5_get_in_tkt_with_keytab(krb5_context context, krb5_flags options,
                            krb5_address *const *addrs, krb5_enctype *ktypes,
                            krb5_preauthtype *pre_auth_types,
                            krb5_keytab arg_keytab, krb5_ccache ccache,
                            krb5_creds *creds, krb5_kdc_rep **ret_as_reply)
{
    krb5_error_code retval;
    krb5_get_init_creds_opt *opts;
    char * server = NULL;
    krb5_keytab keytab;
    krb5_principal client_princ, server_princ;
    int use_master = 0;

    retval = k5_populate_gic_opt(context, &opts, options, addrs, ktypes,
                                 pre_auth_types, creds);
    if (retval)
        return retval;

    if (arg_keytab == NULL) {
        retval = krb5_kt_default(context, &keytab);
        if (retval)
            goto cleanup;
    }
    else keytab = arg_keytab;

    retval = krb5_unparse_name( context, creds->server, &server);
    if (retval)
        goto cleanup;
    server_princ = creds->server;
    client_princ = creds->client;
    retval = k5_get_init_creds(context, creds, creds->client,
                               krb5_prompter_posix,  NULL, 0, server, opts,
                               get_as_key_keytab, (void *)keytab, &use_master,
                               ret_as_reply);
    krb5_free_unparsed_name( context, server);
    if (retval) {
        goto cleanup;
    }
    krb5_free_principal(context, creds->server);
    krb5_free_principal(context, creds->client);
    creds->client = client_princ;
    creds->server = server_princ;

    /* store it in the ccache! */
    if (ccache)
        if ((retval = krb5_cc_store_cred(context, ccache, creds)))
            goto cleanup;
cleanup:
    krb5_get_init_creds_opt_free(context, opts);
    if (arg_keytab == NULL)
        krb5_kt_close(context, keytab);
    return retval;
}

#endif /* LEAN_CLIENT */
