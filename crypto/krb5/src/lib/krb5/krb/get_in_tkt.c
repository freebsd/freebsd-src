/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/get_in_tkt.c */
/*
 * Copyright 1990,1991, 2003, 2008 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "int-proto.h"
#include "os-proto.h"
#include "fast.h"
#include "init_creds_ctx.h"

/* some typedef's for the function args to make things look a bit cleaner */

static krb5_error_code make_preauth_list (krb5_context,
                                          krb5_preauthtype *,
                                          int, krb5_pa_data ***);
static krb5_error_code sort_krb5_padata_sequence(krb5_context context,
                                                 krb5_data *realm,
                                                 krb5_pa_data **padata);

/*
 * Decrypt the AS reply in ctx, populating ctx->reply->enc_part2.  If
 * strengthen_key is not null, combine it with the reply key as specified in
 * RFC 6113 section 5.4.3.  Place the key used in *key_out.
 */
static krb5_error_code
decrypt_as_reply(krb5_context context, krb5_init_creds_context ctx,
                 const krb5_keyblock *strengthen_key, krb5_keyblock *key_out)
{
    krb5_error_code ret;
    krb5_keyblock key;
    krb5_responder_fn responder;
    void *responder_data;

    memset(key_out, 0, sizeof(*key_out));
    memset(&key, 0, sizeof(key));

    if (ctx->as_key.length) {
        /* The reply key was computed or replaced during preauth processing;
         * try it. */
        TRACE_INIT_CREDS_AS_KEY_PREAUTH(context, &ctx->as_key);
        ret = krb5int_fast_reply_key(context, strengthen_key, &ctx->as_key,
                                     &key);
        if (ret)
            return ret;
        ret = krb5_kdc_rep_decrypt_proc(context, &key, NULL, ctx->reply);
        if (!ret) {
            *key_out = key;
            return 0;
        }
        krb5_free_keyblock_contents(context, &key);
        TRACE_INIT_CREDS_PREAUTH_DECRYPT_FAIL(context, ret);

        /*
         * For two reasons, we fall back to trying or retrying the gak_fct if
         * this fails:
         *
         * 1. The KDC might encrypt the reply using a different enctype than
         *    the AS key we computed during preauth.
         *
         * 2. For 1.1.1 and prior KDC's, when SAM is used with USE_SAD_AS_KEY,
         *    the AS-REP is encrypted in the client long-term key instead of
         *    the SAD.
         *
         * The gak_fct for krb5_get_init_creds_with_password() caches the
         * password, so this fallback does not result in a second password
         * prompt.
         */
    } else {
        /*
         * No AS key was computed during preauth processing, perhaps because
         * preauth was not used.  If the caller supplied a responder callback,
         * possibly invoke it before calling the gak_fct for real.
         */
        k5_gic_opt_get_responder(ctx->opt, &responder, &responder_data);
        if (responder != NULL) {
            /* Indicate a need for the AS key by calling the gak_fct with a
             * NULL as_key. */
            ret = ctx->gak_fct(context, ctx->request->client, ctx->etype, NULL,
                               NULL, NULL, NULL, NULL, ctx->gak_data,
                               ctx->rctx.items);
            if (ret)
                return ret;

            /* If that produced a responder question, invoke the responder. */
            if (!k5_response_items_empty(ctx->rctx.items)) {
                ret = (*responder)(context, responder_data, &ctx->rctx);
                if (ret)
                    return ret;
            }
        }
    }

    /* Compute or re-compute the AS key, prompting for the password if
     * necessary. */
    TRACE_INIT_CREDS_GAK(context, &ctx->salt, &ctx->s2kparams);
    ret = ctx->gak_fct(context, ctx->request->client,
                       ctx->reply->enc_part.enctype, ctx->prompter,
                       ctx->prompter_data, &ctx->salt, &ctx->s2kparams,
                       &ctx->as_key, ctx->gak_data, ctx->rctx.items);
    if (ret)
        return ret;
    TRACE_INIT_CREDS_AS_KEY_GAK(context, &ctx->as_key);

    ret = krb5int_fast_reply_key(context, strengthen_key, &ctx->as_key, &key);
    if (ret)
        return ret;
    ret = krb5_kdc_rep_decrypt_proc(context, &key, NULL, ctx->reply);
    if (ret) {
        krb5_free_keyblock_contents(context, &key);
        return ret;
    }

    *key_out = key;
    return 0;
}

/**
 * Fully anonymous replies include a pa_pkinit_kx padata type including the KDC
 * contribution key.  This routine confirms that the session key is of the
 * right form for fully anonymous requests.  It is here rather than in the
 * preauth code because the session key cannot be verified until the AS reply
 * is decrypted and the preauth code all runs before the AS reply is decrypted.
 */
static krb5_error_code
verify_anonymous( krb5_context context, krb5_kdc_req *request,
                  krb5_kdc_rep *reply, krb5_keyblock *as_key)
{
    krb5_error_code ret = 0;
    krb5_pa_data *pa;
    krb5_data scratch;
    krb5_keyblock *kdc_key = NULL, *expected = NULL;
    krb5_enc_data *enc = NULL;
    krb5_keyblock *session = reply->enc_part2->session;

    if (!krb5_principal_compare_any_realm(context, request->client,
                                          krb5_anonymous_principal()))
        return 0; /* Only applies to fully anonymous */
    pa = krb5int_find_pa_data(context, reply->padata, KRB5_PADATA_PKINIT_KX);
    if (pa == NULL)
        goto verification_error;
    scratch.length = pa->length;
    scratch.data = (char *) pa->contents;
    ret = decode_krb5_enc_data( &scratch, &enc);
    if (ret)
        goto cleanup;
    scratch.data = k5alloc(enc->ciphertext.length, &ret);
    if (ret)
        goto cleanup;
    scratch.length = enc->ciphertext.length;
    ret = krb5_c_decrypt(context, as_key, KRB5_KEYUSAGE_PA_PKINIT_KX,
                         NULL /*cipherstate*/, enc, &scratch);
    if (ret) {
        free(scratch.data);
        goto cleanup;
    }
    ret = decode_krb5_encryption_key( &scratch, &kdc_key);
    zap(scratch.data, scratch.length);
    free(scratch.data);
    if (ret)
        goto cleanup;
    ret = krb5_c_fx_cf2_simple(context, kdc_key, "PKINIT",
                               as_key, "KEYEXCHANGE", &expected);
    if (ret)
        goto cleanup;
    if ((expected->enctype != session->enctype) ||
        (expected->length != session->length) ||
        (memcmp(expected->contents, session->contents, expected->length) != 0))
        goto verification_error;
cleanup:
    if (kdc_key)
        krb5_free_keyblock(context, kdc_key);
    if (expected)
        krb5_free_keyblock(context, expected);
    if (enc)
        krb5_free_enc_data(context, enc);
    return ret;
verification_error:
    ret = KRB5_KDCREP_MODIFIED;
    k5_setmsg(context, ret,
              _("Reply has wrong form of session key for anonymous request"));
    goto cleanup;
}

static krb5_error_code
verify_as_reply(krb5_context            context,
                krb5_timestamp          time_now,
                krb5_kdc_req            *request,
                krb5_kdc_rep            *as_reply)
{
    krb5_error_code             retval;
    int                         canon_req;
    int                         canon_ok;
    krb5_timestamp              time_offset;

    /* check the contents for sanity: */
    if (!as_reply->enc_part2->times.starttime)
        as_reply->enc_part2->times.starttime =
            as_reply->enc_part2->times.authtime;

    /*
     * We only allow the AS-REP server name to be changed if the
     * caller set the canonicalize flag (or requested an enterprise
     * principal) and we requested (and received) a TGT.
     */
    canon_req = ((request->kdc_options & KDC_OPT_CANONICALIZE) != 0) ||
        request->client->type == KRB5_NT_ENTERPRISE_PRINCIPAL ||
        (request->kdc_options & KDC_OPT_REQUEST_ANONYMOUS);
    if (canon_req) {
        canon_ok = IS_TGS_PRINC(request->server) &&
            IS_TGS_PRINC(as_reply->enc_part2->server);
    } else
        canon_ok = 0;

    if ((!canon_ok &&
         !krb5_principal_compare(context, as_reply->enc_part2->server, request->server))
        || (!canon_req && !krb5_principal_compare(context, as_reply->client, request->client))
        || !krb5_principal_compare(context, as_reply->enc_part2->server, as_reply->ticket->server)
        || (request->nonce != as_reply->enc_part2->nonce)
        /* XXX check for extraneous flags */
        /* XXX || (!krb5_addresses_compare(context, addrs, as_reply->enc_part2->caddrs)) */
        || ((request->kdc_options & KDC_OPT_POSTDATED) &&
            (request->from != 0) &&
            (request->from != as_reply->enc_part2->times.starttime))
        || ((request->till != 0) &&
            ts_after(as_reply->enc_part2->times.endtime, request->till))
        || ((request->kdc_options & KDC_OPT_RENEWABLE) &&
            (request->rtime != 0) &&
            ts_after(as_reply->enc_part2->times.renew_till, request->rtime))
        || ((request->kdc_options & KDC_OPT_RENEWABLE_OK) &&
            !(request->kdc_options & KDC_OPT_RENEWABLE) &&
            (as_reply->enc_part2->flags & KDC_OPT_RENEWABLE) &&
            (request->till != 0) &&
            ts_after(as_reply->enc_part2->times.renew_till, request->till))
    ) {
        return KRB5_KDCREP_MODIFIED;
    }

    if (context->library_options & KRB5_LIBOPT_SYNC_KDCTIME) {
        time_offset = ts_delta(as_reply->enc_part2->times.authtime, time_now);
        retval = krb5_set_time_offsets(context, time_offset, 0);
        if (retval)
            return retval;
    } else {
        if ((request->from == 0) &&
            !ts_within(as_reply->enc_part2->times.starttime, time_now,
                       context->clockskew))
            return (KRB5_KDCREP_SKEW);
    }
    return 0;
}

static krb5_error_code
stash_as_reply(krb5_context             context,
               krb5_kdc_rep             *as_reply,
               krb5_creds *             creds,
               krb5_ccache              ccache)
{
    krb5_error_code             retval;
    krb5_data *                 packet;
    krb5_principal              client;
    krb5_principal              server;

    client = NULL;
    server = NULL;

    if (!creds->client)
        if ((retval = krb5_copy_principal(context, as_reply->client, &client)))
            goto cleanup;

    if (!creds->server)
        if ((retval = krb5_copy_principal(context, as_reply->enc_part2->server,
                                          &server)))
            goto cleanup;

    /* fill in the credentials */
    if ((retval = krb5_copy_keyblock_contents(context,
                                              as_reply->enc_part2->session,
                                              &creds->keyblock)))
        goto cleanup;

    creds->times = as_reply->enc_part2->times;
    creds->is_skey = FALSE;             /* this is an AS_REQ, so cannot
                                           be encrypted in skey */
    creds->ticket_flags = as_reply->enc_part2->flags;
    if ((retval = krb5_copy_addresses(context, as_reply->enc_part2->caddrs,
                                      &creds->addresses)))
        goto cleanup;

    creds->second_ticket.length = 0;
    creds->second_ticket.data = 0;

    if ((retval = encode_krb5_ticket(as_reply->ticket, &packet)))
        goto cleanup;

    creds->ticket = *packet;
    free(packet);

    /* store it in the ccache! */
    if (ccache)
        if ((retval = krb5_cc_store_cred(context, ccache, creds)))
            goto cleanup;

    if (!creds->client)
        creds->client = client;
    if (!creds->server)
        creds->server = server;

cleanup:
    if (retval) {
        if (client)
            krb5_free_principal(context, client);
        if (server)
            krb5_free_principal(context, server);
        if (creds->keyblock.contents) {
            memset(creds->keyblock.contents, 0,
                   creds->keyblock.length);
            free(creds->keyblock.contents);
            creds->keyblock.contents = 0;
            creds->keyblock.length = 0;
        }
        if (creds->ticket.data) {
            free(creds->ticket.data);
            creds->ticket.data = 0;
        }
        if (creds->addresses) {
            krb5_free_addresses(context, creds->addresses);
            creds->addresses = 0;
        }
    }
    return (retval);
}

static krb5_error_code
make_preauth_list(krb5_context  context,
                  krb5_preauthtype *    ptypes,
                  int                   nptypes,
                  krb5_pa_data ***      ret_list)
{
    krb5_preauthtype *          ptypep;
    krb5_pa_data **             preauthp;
    int                         i;

    if (nptypes < 0) {
        for (nptypes=0, ptypep = ptypes; *ptypep; ptypep++, nptypes++)
            ;
    }

    /* allocate space for a NULL to terminate the list */

    if ((preauthp =
         (krb5_pa_data **) malloc((nptypes+1)*sizeof(krb5_pa_data *))) == NULL)
        return(ENOMEM);

    for (i=0; i<nptypes; i++) {
        if ((preauthp[i] =
             (krb5_pa_data *) malloc(sizeof(krb5_pa_data))) == NULL) {
            for (; i>=0; i--)
                free(preauthp[i]);
            free(preauthp);
            return (ENOMEM);
        }
        preauthp[i]->magic = KV5M_PA_DATA;
        preauthp[i]->pa_type = ptypes[i];
        preauthp[i]->length = 0;
        preauthp[i]->contents = 0;
    }

    /* fill in the terminating NULL */

    preauthp[nptypes] = NULL;

    *ret_list = preauthp;
    return 0;
}

#define MAX_IN_TKT_LOOPS 16

/* Sort a pa_data sequence so that types named in the "preferred_preauth_types"
 * libdefaults entry are listed before any others. */
static krb5_error_code
sort_krb5_padata_sequence(krb5_context context, krb5_data *realm,
                          krb5_pa_data **padata)
{
    int i, j, base;
    krb5_error_code ret;
    const char *p;
    long l;
    char *q, *preauth_types = NULL;
    krb5_pa_data *tmp;
    int need_free_string = 1;

    if ((padata == NULL) || (padata[0] == NULL)) {
        return 0;
    }

    ret = krb5int_libdefault_string(context, realm, KRB5_CONF_PREFERRED_PREAUTH_TYPES,
                                    &preauth_types);
    if ((ret != 0) || (preauth_types == NULL)) {
        /* Try to use PKINIT first. */
        preauth_types = "17, 16, 15, 14";
        need_free_string = 0;
    }

#ifdef DEBUG
    fprintf (stderr, "preauth data types before sorting:");
    for (i = 0; padata[i]; i++) {
        fprintf (stderr, " %d", padata[i]->pa_type);
    }
    fprintf (stderr, "\n");
#endif

    base = 0;
    for (p = preauth_types; *p != '\0';) {
        /* skip whitespace to find an entry */
        p += strspn(p, ", ");
        if (*p != '\0') {
            /* see if we can extract a number */
            l = strtol(p, &q, 10);
            if ((q != NULL) && (q > p)) {
                /* got a valid number; search for a matching entry */
                for (i = base; padata[i] != NULL; i++) {
                    /* bubble the matching entry to the front of the list */
                    if (padata[i]->pa_type == l) {
                        tmp = padata[i];
                        for (j = i; j > base; j--)
                            padata[j] = padata[j - 1];
                        padata[base] = tmp;
                        base++;
                        break;
                    }
                }
                p = q;
            } else {
                break;
            }
        }
    }
    if (need_free_string)
        free(preauth_types);

#ifdef DEBUG
    fprintf (stderr, "preauth data types after sorting:");
    for (i = 0; padata[i]; i++)
        fprintf (stderr, " %d", padata[i]->pa_type);
    fprintf (stderr, "\n");
#endif

    return 0;
}

static krb5_error_code
build_in_tkt_name(krb5_context context,
                  const char *in_tkt_service,
                  krb5_const_principal client,
                  krb5_principal *server_out)
{
    krb5_error_code ret;
    krb5_principal server = NULL;

    *server_out = NULL;

    if (in_tkt_service) {
        ret = krb5_parse_name_flags(context, in_tkt_service,
                                    KRB5_PRINCIPAL_PARSE_IGNORE_REALM,
                                    &server);
        if (ret)
            return ret;
        krb5_free_data_contents(context, &server->realm);
        ret = krb5int_copy_data_contents(context, &client->realm,
                                         &server->realm);
        if (ret) {
            krb5_free_principal(context, server);
            return ret;
        }
    } else {
        ret = krb5_build_principal_ext(context, &server,
                                       client->realm.length,
                                       client->realm.data,
                                       KRB5_TGS_NAME_SIZE,
                                       KRB5_TGS_NAME,
                                       client->realm.length,
                                       client->realm.data,
                                       0);
        if (ret)
            return ret;
    }

    *server_out = server;
    return 0;
}

void KRB5_CALLCONV
krb5_init_creds_free(krb5_context context,
                     krb5_init_creds_context ctx)
{
    if (ctx == NULL)
        return;

    k5_response_items_free(ctx->rctx.items);
    free(ctx->in_tkt_service);
    zapfree(ctx->gakpw.storage.data, ctx->gakpw.storage.length);
    k5_preauth_request_context_fini(context, ctx);
    krb5_free_error(context, ctx->err_reply);
    krb5_free_pa_data(context, ctx->err_padata);
    krb5_free_cred_contents(context, &ctx->cred);
    krb5_free_kdc_req(context, ctx->request);
    krb5_free_kdc_rep(context, ctx->reply);
    krb5_free_data(context, ctx->outer_request_body);
    krb5_free_data(context, ctx->inner_request_body);
    krb5_free_data(context, ctx->encoded_previous_request);
    krb5int_fast_free_state(context, ctx->fast_state);
    krb5_free_pa_data(context, ctx->optimistic_padata);
    krb5_free_pa_data(context, ctx->method_padata);
    krb5_free_pa_data(context, ctx->more_padata);
    krb5_free_data_contents(context, &ctx->salt);
    krb5_free_data_contents(context, &ctx->s2kparams);
    krb5_free_keyblock_contents(context, &ctx->as_key);
    k5_json_release(ctx->cc_config_in);
    k5_json_release(ctx->cc_config_out);
    free(ctx);
}

krb5_error_code
k5_init_creds_get(krb5_context context, krb5_init_creds_context ctx,
                  int *use_primary)
{
    krb5_error_code code;
    krb5_data request;
    krb5_data reply;
    krb5_data realm;
    unsigned int flags = 0;
    int tcp_only = 0, primary = *use_primary;

    request.length = 0;
    request.data = NULL;
    reply.length = 0;
    reply.data = NULL;
    realm.length = 0;
    realm.data = NULL;

    for (;;) {
        code = krb5_init_creds_step(context,
                                    ctx,
                                    &reply,
                                    &request,
                                    &realm,
                                    &flags);
        if (code == KRB5KRB_ERR_RESPONSE_TOO_BIG && !tcp_only) {
            TRACE_INIT_CREDS_RETRY_TCP(context);
            tcp_only = 1;
        } else if (code != 0 || !(flags & KRB5_INIT_CREDS_STEP_FLAG_CONTINUE))
            break;

        krb5_free_data_contents(context, &reply);

        primary = *use_primary;
        code = krb5_sendto_kdc(context, &request, &realm,
                               &reply, &primary, tcp_only);
        if (code != 0)
            break;

        krb5_free_data_contents(context, &request);
        krb5_free_data_contents(context, &realm);
    }

    krb5_free_data_contents(context, &request);
    krb5_free_data_contents(context, &reply);
    krb5_free_data_contents(context, &realm);

    *use_primary = primary;
    return code;
}

/* Heimdal API */
krb5_error_code KRB5_CALLCONV
krb5_init_creds_get(krb5_context context,
                    krb5_init_creds_context ctx)
{
    int use_primary = 0;

    return k5_init_creds_get(context, ctx, &use_primary);
}

krb5_error_code KRB5_CALLCONV
krb5_init_creds_get_creds(krb5_context context,
                          krb5_init_creds_context ctx,
                          krb5_creds *creds)
{
    if (!ctx->complete)
        return KRB5_NO_TKT_SUPPLIED;

    return k5_copy_creds_contents(context, &ctx->cred, creds);
}

krb5_error_code KRB5_CALLCONV
krb5_init_creds_get_times(krb5_context context,
                          krb5_init_creds_context ctx,
                          krb5_ticket_times *times)
{
    if (!ctx->complete)
        return KRB5_NO_TKT_SUPPLIED;

    *times = ctx->cred.times;

    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_init_creds_get_error(krb5_context context,
                          krb5_init_creds_context ctx,
                          krb5_error **error)
{
    krb5_error_code code;
    krb5_error *ret = NULL;

    *error = NULL;

    if (ctx->err_reply == NULL)
        return 0;

    ret = k5alloc(sizeof(*ret), &code);
    if (code != 0)
        goto cleanup;

    ret->magic = KV5M_ERROR;
    ret->ctime = ctx->err_reply->ctime;
    ret->cusec = ctx->err_reply->cusec;
    ret->susec = ctx->err_reply->susec;
    ret->stime = ctx->err_reply->stime;
    ret->error = ctx->err_reply->error;

    if (ctx->err_reply->client != NULL) {
        code = krb5_copy_principal(context, ctx->err_reply->client,
                                   &ret->client);
        if (code != 0)
            goto cleanup;
    }

    code = krb5_copy_principal(context, ctx->err_reply->server, &ret->server);
    if (code != 0)
        goto cleanup;

    code = krb5int_copy_data_contents(context, &ctx->err_reply->text,
                                      &ret->text);
    if (code != 0)
        goto cleanup;

    code = krb5int_copy_data_contents(context, &ctx->err_reply->e_data,
                                      &ret->e_data);
    if (code != 0)
        goto cleanup;

    *error = ret;

cleanup:
    if (code != 0)
        krb5_free_error(context, ret);

    return code;
}

/* Return the current time, possibly using the offset from a previously
 * received preauth-required error. */
krb5_error_code
k5_init_creds_current_time(krb5_context context, krb5_init_creds_context ctx,
                           krb5_boolean allow_unauth, krb5_timestamp *time_out,
                           krb5_int32 *usec_out)
{
    if (ctx->pa_offset_state != NO_OFFSET &&
        (allow_unauth || ctx->pa_offset_state == AUTH_OFFSET) &&
        (context->library_options & KRB5_LIBOPT_SYNC_KDCTIME)) {
        /* Use the offset we got from a preauth-required error. */
        return k5_time_with_offset(ctx->pa_offset, ctx->pa_offset_usec,
                                   time_out, usec_out);
    } else {
        /* Use the time offset from the context, or no offset. */
        return krb5_us_timeofday(context, time_out, usec_out);
    }
}

/* Set the timestamps for ctx->request based on the desired lifetimes. */
static krb5_error_code
set_request_times(krb5_context context, krb5_init_creds_context ctx)
{
    krb5_error_code code;
    krb5_timestamp from, now;
    krb5_int32 now_ms;

    code = k5_init_creds_current_time(context, ctx, TRUE, &now, &now_ms);
    if (code != 0)
        return code;

    /* Omit request start time unless the caller explicitly asked for one. */
    from = ts_incr(now, ctx->start_time);
    if (ctx->start_time != 0)
        ctx->request->from = from;

    ctx->request->till = ts_incr(from, ctx->tkt_life);

    if (ctx->renew_life > 0) {
        /* Don't ask for a smaller renewable time than the lifetime. */
        ctx->request->rtime = ts_incr(from, ctx->renew_life);
        if (ts_after(ctx->request->till, ctx->request->rtime))
            ctx->request->rtime = ctx->request->till;
        ctx->request->kdc_options &= ~KDC_OPT_RENEWABLE_OK;
    } else {
        ctx->request->rtime = 0;
    }

    return 0;
}

static void
read_allowed_preauth_type(krb5_context context, krb5_init_creds_context ctx)
{
    krb5_error_code ret;
    krb5_data config;
    char *tmp, *p;
    krb5_ccache in_ccache = k5_gic_opt_get_in_ccache(ctx->opt);

    ctx->allowed_preauth_type = KRB5_PADATA_NONE;
    if (in_ccache == NULL)
        return;
    memset(&config, 0, sizeof(config));
    if (krb5_cc_get_config(context, in_ccache, ctx->request->server,
                           KRB5_CC_CONF_PA_TYPE, &config) != 0)
        return;
    tmp = k5memdup0(config.data, config.length, &ret);
    krb5_free_data_contents(context, &config);
    if (tmp == NULL)
        return;
    ctx->allowed_preauth_type = strtol(tmp, &p, 10);
    if (p == NULL || *p != '\0')
        ctx->allowed_preauth_type = KRB5_PADATA_NONE;
    free(tmp);
}

/* Return true if encrypted timestamp is disabled for realm. */
static krb5_boolean
encts_disabled(profile_t profile, const krb5_data *realm)
{
    krb5_error_code ret;
    char *realmstr;
    int bval;

    realmstr = k5memdup0(realm->data, realm->length, &ret);
    if (realmstr == NULL)
        return FALSE;
    ret = profile_get_boolean(profile, KRB5_CONF_REALMS, realmstr,
                              KRB5_CONF_DISABLE_ENCRYPTED_TIMESTAMP, FALSE,
                              &bval);
    free(realmstr);
    return (ret == 0) ? bval : FALSE;
}

/**
 * Throw away any pre-authentication realm state and begin with a
 * unauthenticated or optimistically authenticated request.  If fast_upgrade is
 * set, use FAST for this request.
 */
static krb5_error_code
restart_init_creds_loop(krb5_context context, krb5_init_creds_context ctx,
                        krb5_boolean fast_upgrade)
{
    krb5_error_code code = 0;

    krb5_free_pa_data(context, ctx->optimistic_padata);
    krb5_free_pa_data(context, ctx->method_padata);
    krb5_free_pa_data(context, ctx->more_padata);
    krb5_free_pa_data(context, ctx->err_padata);
    krb5_free_error(context, ctx->err_reply);
    ctx->optimistic_padata = ctx->method_padata = ctx->more_padata = NULL;
    ctx->err_padata = NULL;
    ctx->err_reply = NULL;
    ctx->selected_preauth_type = KRB5_PADATA_NONE;

    krb5int_fast_free_state(context, ctx->fast_state);
    ctx->fast_state = NULL;
    code = krb5int_fast_make_state(context, &ctx->fast_state);
    if (code != 0)
        goto cleanup;
    if (fast_upgrade)
        ctx->fast_state->fast_state_flags |= KRB5INT_FAST_DO_FAST;

    k5_preauth_request_context_fini(context, ctx);
    k5_preauth_request_context_init(context, ctx);
    krb5_free_data(context, ctx->outer_request_body);
    ctx->outer_request_body = NULL;
    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST) {
        code = make_preauth_list(context, ctx->opt->preauth_list,
                                 ctx->opt->preauth_list_length,
                                 &ctx->optimistic_padata);
        if (code)
            goto cleanup;
    }

    /* Never set encts_disabled back to false, so it can't be circumvented with
     * client realm referrals. */
    if (encts_disabled(context->profile, &ctx->request->client->realm))
        ctx->encts_disabled = TRUE;

    krb5_free_principal(context, ctx->request->server);
    ctx->request->server = NULL;

    code = build_in_tkt_name(context, ctx->in_tkt_service,
                             ctx->request->client,
                             &ctx->request->server);
    if (code != 0)
        goto cleanup;

    code = krb5int_fast_as_armor(context, ctx->fast_state, ctx->opt,
                                 ctx->request);
    if (code != 0)
        goto cleanup;
    /* give the preauth plugins a chance to prep the request body */
    k5_preauth_prepare_request(context, ctx->opt, ctx->request);

    code = krb5int_fast_prep_req_body(context, ctx->fast_state,
                                      ctx->request,
                                      &ctx->outer_request_body);
    if (code != 0)
        goto cleanup;

    /* Read the allowed preauth type for this server principal from the input
     * ccache, if the application supplied one. */
    read_allowed_preauth_type(context, ctx);

cleanup:
    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_init_creds_init(krb5_context context,
                     krb5_principal client,
                     krb5_prompter_fct prompter,
                     void *data,
                     krb5_deltat start_time,
                     krb5_get_init_creds_opt *opt,
                     krb5_init_creds_context *pctx)
{
    krb5_error_code code;
    krb5_init_creds_context ctx;
    int tmp;
    char *str = NULL;

    TRACE_INIT_CREDS(context, client);

    ctx = k5alloc(sizeof(*ctx), &code);
    if (code != 0)
        goto cleanup;

    ctx->request = k5alloc(sizeof(krb5_kdc_req), &code);
    if (code != 0)
        goto cleanup;
    ctx->info_pa_permitted = TRUE;
    code = krb5_copy_principal(context, client, &ctx->request->client);
    if (code != 0)
        goto cleanup;

    ctx->prompter = prompter;
    ctx->prompter_data = data;
    ctx->gak_fct = krb5_get_as_key_password;
    ctx->gak_data = &ctx->gakpw;

    ctx->start_time = start_time;

    if (opt == NULL) {
        ctx->opt = &ctx->opt_storage;
        krb5_get_init_creds_opt_init(ctx->opt);
    } else {
        ctx->opt = opt;
    }

    code = k5_response_items_new(&ctx->rctx.items);
    if (code != 0)
        goto cleanup;

    /* Initialise request parameters as per krb5_get_init_creds() */
    ctx->request->kdc_options = context->kdc_default_options;

    /* forwardable */
    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_FORWARDABLE)
        tmp = ctx->opt->forwardable;
    else if (krb5int_libdefault_boolean(context, &ctx->request->client->realm,
                                        KRB5_CONF_FORWARDABLE, &tmp) == 0)
        ;
    else
        tmp = 0;
    if (tmp)
        ctx->request->kdc_options |= KDC_OPT_FORWARDABLE;

    /* proxiable */
    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_PROXIABLE)
        tmp = ctx->opt->proxiable;
    else if (krb5int_libdefault_boolean(context, &ctx->request->client->realm,
                                        KRB5_CONF_PROXIABLE, &tmp) == 0)
        ;
    else
        tmp = 0;
    if (tmp)
        ctx->request->kdc_options |= KDC_OPT_PROXIABLE;

    /* canonicalize */
    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_CANONICALIZE)
        tmp = 1;
    else if (krb5int_libdefault_boolean(context, &ctx->request->client->realm,
                                        KRB5_CONF_CANONICALIZE, &tmp) == 0)
        ;
    else
        tmp = 0;
    if (tmp)
        ctx->request->kdc_options |= KDC_OPT_CANONICALIZE;

    /* allow_postdate */
    if (ctx->start_time > 0)
        ctx->request->kdc_options |= KDC_OPT_ALLOW_POSTDATE | KDC_OPT_POSTDATED;

    /* ticket lifetime */
    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_TKT_LIFE)
        ctx->tkt_life = ctx->opt->tkt_life;
    else if (krb5int_libdefault_string(context, &ctx->request->client->realm,
                                       KRB5_CONF_TICKET_LIFETIME, &str) == 0) {
        code = krb5_string_to_deltat(str, &ctx->tkt_life);
        if (code != 0)
            goto cleanup;
        free(str);
        str = NULL;
    } else
        ctx->tkt_life = 24 * 60 * 60; /* previously hardcoded in kinit */

    /* renewable lifetime */
    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE)
        ctx->renew_life = ctx->opt->renew_life;
    else if (krb5int_libdefault_string(context, &ctx->request->client->realm,
                                       KRB5_CONF_RENEW_LIFETIME, &str) == 0) {
        code = krb5_string_to_deltat(str, &ctx->renew_life);
        if (code != 0)
            goto cleanup;
        free(str);
        str = NULL;
    } else
        ctx->renew_life = 0;

    if (ctx->renew_life > 0)
        ctx->request->kdc_options |= KDC_OPT_RENEWABLE;

    /* enctypes */
    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST) {
        ctx->request->ktype =
            k5memdup(ctx->opt->etype_list,
                     ctx->opt->etype_list_length * sizeof(krb5_enctype),
                     &code);
        if (code != 0)
            goto cleanup;
        ctx->request->nktypes = ctx->opt->etype_list_length;
    } else if (krb5_get_default_in_tkt_ktypes(context,
                                              &ctx->request->ktype) == 0) {
        ctx->request->nktypes = k5_count_etypes(ctx->request->ktype);
    } else {
        /* there isn't any useful default here. */
        code = KRB5_CONFIG_ETYPE_NOSUPP;
        goto cleanup;
    }

    /*
     * Set a default enctype for optimistic preauth.  If we're not doing
     * optimistic preauth, this should ordinarily get overwritten when we
     * process the etype-info2 of the preauth-required error.
     */
    if (ctx->request->nktypes > 0)
        ctx->etype = ctx->request->ktype[0];

    /* addresses */
    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST) {
        code = krb5_copy_addresses(context, ctx->opt->address_list,
                                   &ctx->request->addresses);
        if (code != 0)
            goto cleanup;
    } else if (krb5int_libdefault_boolean(context, &ctx->request->client->realm,
                                          KRB5_CONF_NOADDRESSES, &tmp) != 0
               || tmp) {
        ctx->request->addresses = NULL;
    } else {
        code = krb5_os_localaddr(context, &ctx->request->addresses);
        if (code != 0)
            goto cleanup;
    }

    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_SALT) {
        code = krb5int_copy_data_contents(context, ctx->opt->salt, &ctx->salt);
        if (code != 0)
            goto cleanup;
        ctx->default_salt = FALSE;
    } else {
        ctx->salt = empty_data();
        ctx->default_salt = TRUE;
    }

    /* Anonymous. */
    if (ctx->opt->flags & KRB5_GET_INIT_CREDS_OPT_ANONYMOUS) {
        ctx->request->kdc_options |= KDC_OPT_REQUEST_ANONYMOUS;
        /* Remap @REALM to WELLKNOWN/ANONYMOUS@REALM. */
        if (client->length == 1 && client->data[0].length ==0) {
            krb5_principal new_client;
            code = krb5_build_principal_ext(context, &new_client,
                                            client->realm.length,
                                            client->realm.data,
                                            strlen(KRB5_WELLKNOWN_NAMESTR),
                                            KRB5_WELLKNOWN_NAMESTR,
                                            strlen(KRB5_ANONYMOUS_PRINCSTR),
                                            KRB5_ANONYMOUS_PRINCSTR,
                                            0);
            if (code)
                goto cleanup;
            krb5_free_principal(context, ctx->request->client);
            ctx->request->client = new_client;
            ctx->request->client->type = KRB5_NT_WELLKNOWN;
        }
    }
    /* We will also handle anonymous if the input principal is the anonymous
     * principal. */
    if (krb5_principal_compare_any_realm(context, ctx->request->client,
                                         krb5_anonymous_principal())) {
        ctx->request->kdc_options |= KDC_OPT_REQUEST_ANONYMOUS;
        ctx->request->client->type = KRB5_NT_WELLKNOWN;
    }

    *pctx = ctx;
    ctx = NULL;

cleanup:
    krb5_init_creds_free(context, ctx);
    free(str);

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_init_creds_set_service(krb5_context context,
                            krb5_init_creds_context ctx,
                            const char *service)
{
    char *s;

    TRACE_INIT_CREDS_SERVICE(context, service);

    s = strdup(service);
    if (s == NULL)
        return ENOMEM;

    free(ctx->in_tkt_service);
    ctx->in_tkt_service = s;

    return restart_init_creds_loop(context, ctx, FALSE);
}

static krb5_error_code
init_creds_validate_reply(krb5_context context,
                          krb5_init_creds_context ctx,
                          krb5_data *reply)
{
    krb5_error_code code;
    krb5_error *error = NULL;
    krb5_kdc_rep *as_reply = NULL;

    krb5_free_error(context, ctx->err_reply);
    ctx->err_reply = NULL;

    krb5_free_kdc_rep(context, ctx->reply);
    ctx->reply = NULL;

    if (krb5_is_krb_error(reply)) {
        code = decode_krb5_error(reply, &error);
        if (code != 0)
            return code;

        assert(error != NULL);

        TRACE_INIT_CREDS_ERROR_REPLY(context,
                                     error->error + ERROR_TABLE_BASE_krb5);
        if (error->error == KRB_ERR_RESPONSE_TOO_BIG) {
            krb5_free_error(context, error);
            return KRB5KRB_ERR_RESPONSE_TOO_BIG;
        } else {
            ctx->err_reply = error;
            return 0;
        }
    }

    /*
     * Check to make sure it isn't a V4 reply.
     */
    if (reply->length != 0 && !krb5_is_as_rep(reply)) {
/* these are in <kerberosIV/prot.h> as well but it isn't worth including. */
#define V4_KRB_PROT_VERSION     4
#define V4_AUTH_MSG_ERR_REPLY   (5<<1)
        /* check here for V4 reply */
        unsigned int t_switch;

        /* From v4 g_in_tkt.c: This used to be
           switch (pkt_msg_type(rpkt) & ~1) {
           but SCO 3.2v4 cc compiled that incorrectly.  */
        t_switch = reply->data[1];
        t_switch &= ~1;

        if (t_switch == V4_AUTH_MSG_ERR_REPLY
            && reply->data[0] == V4_KRB_PROT_VERSION) {
            code = KRB5KRB_AP_ERR_V4_REPLY;
        } else {
            code = KRB5KRB_AP_ERR_MSG_TYPE;
        }
        return code;
    }

    /* It must be a KRB_AS_REP message, or an bad returned packet */
    code = decode_krb5_as_rep(reply, &as_reply);
    if (code != 0)
        return code;

    if (as_reply->msg_type != KRB5_AS_REP) {
        krb5_free_kdc_rep(context, as_reply);
        return KRB5KRB_AP_ERR_MSG_TYPE;
    }

    ctx->reply = as_reply;

    return 0;
}

static krb5_error_code
save_selected_preauth_type(krb5_context context, krb5_ccache ccache,
                           krb5_init_creds_context ctx)
{
    krb5_data config_data;
    char *tmp;
    krb5_error_code code;

    if (ctx->selected_preauth_type == KRB5_PADATA_NONE)
        return 0;
    if (asprintf(&tmp, "%ld", (long)ctx->selected_preauth_type) < 0)
        return ENOMEM;
    config_data = string2data(tmp);
    code = krb5_cc_set_config(context, ccache, ctx->cred.server,
                              KRB5_CC_CONF_PA_TYPE, &config_data);
    free(tmp);
    return code;
}

static krb5_error_code
clear_cc_config_out_data(krb5_context context, krb5_init_creds_context ctx)
{
    k5_json_release(ctx->cc_config_out);
    ctx->cc_config_out = NULL;
    return k5_json_object_create(&ctx->cc_config_out);
}

static krb5_error_code
read_cc_config_in_data(krb5_context context, krb5_init_creds_context ctx)
{
    k5_json_value val;
    krb5_data config;
    char *encoded;
    krb5_error_code code;
    krb5_ccache in_ccache = k5_gic_opt_get_in_ccache(ctx->opt);

    k5_json_release(ctx->cc_config_in);
    ctx->cc_config_in = NULL;

    if (in_ccache == NULL)
        return 0;

    memset(&config, 0, sizeof(config));
    code = krb5_cc_get_config(context, in_ccache, ctx->request->server,
                              KRB5_CC_CONF_PA_CONFIG_DATA, &config);
    if (code)
        return code;

    encoded = k5memdup0(config.data, config.length, &code);
    krb5_free_data_contents(context, &config);
    if (encoded == NULL)
        return ENOMEM;

    code = k5_json_decode(encoded, &val);
    free(encoded);
    if (code)
        return code;
    if (k5_json_get_tid(val) != K5_JSON_TID_OBJECT) {
        k5_json_release(val);
        return EINVAL;
    }
    ctx->cc_config_in = val;
    return 0;
}

static krb5_error_code
save_cc_config_out_data(krb5_context context, krb5_ccache ccache,
                        krb5_init_creds_context ctx)
{
    krb5_data config;
    char *encoded;
    krb5_error_code code;

    if (ctx->cc_config_out == NULL ||
        k5_json_object_count(ctx->cc_config_out) == 0)
        return 0;
    code = k5_json_encode(ctx->cc_config_out, &encoded);
    if (code)
        return code;
    config = string2data(encoded);
    code = krb5_cc_set_config(context, ccache, ctx->cred.server,
                              KRB5_CC_CONF_PA_CONFIG_DATA, &config);
    free(encoded);
    return code;
}

/* Add a KERB-PA-PAC-REQUEST pa-data item if the gic options require one. */
static krb5_error_code
maybe_add_pac_request(krb5_context context, krb5_init_creds_context ctx)
{
    krb5_error_code code;
    krb5_pa_pac_req pac_req;
    krb5_data *encoded;
    int val;

    val = k5_gic_opt_pac_request(ctx->opt);
    if (val == -1)
        return 0;

    pac_req.include_pac = val;
    code = encode_krb5_pa_pac_req(&pac_req, &encoded);
    if (code)
        return code;
    code = k5_add_pa_data_from_data(&ctx->request->padata,
                                    KRB5_PADATA_PAC_REQUEST, encoded);
    krb5_free_data(context, encoded);
    return code;
}

static krb5_error_code
init_creds_step_request(krb5_context context,
                        krb5_init_creds_context ctx,
                        krb5_data *out)
{
    krb5_error_code code;
    krb5_preauthtype pa_type;
    krb5_data copy;
    struct errinfo save = EMPTY_ERRINFO;
    uint32_t rcode = (ctx->err_reply == NULL) ? 0 : ctx->err_reply->error;

    if (ctx->loopcount >= MAX_IN_TKT_LOOPS) {
        code = KRB5_GET_IN_TKT_LOOP;
        goto cleanup;
    }

    /* RFC 6113 requires a new nonce for the inner request on each try. */
    code = k5_generate_nonce(context, &ctx->request->nonce);
    if (code != 0)
        goto cleanup;

    /* Reset the request timestamps, possibly adjusting to the KDC time. */
    code = set_request_times(context, ctx);
    if (code != 0)
        goto cleanup;

    krb5_free_data(context, ctx->inner_request_body);
    ctx->inner_request_body = NULL;
    code = encode_krb5_kdc_req_body(ctx->request, &ctx->inner_request_body);
    if (code)
        goto cleanup;

    /*
     * Read cached preauth configuration data for this server principal from
     * the in_ccache, if the application supplied one, and delete any that was
     * stored by a previous (clearly failed) module.
     */
    read_cc_config_in_data(context, ctx);
    clear_cc_config_out_data(context, ctx);

    ctx->request->padata = NULL;
    if (ctx->optimistic_padata != NULL) {
        /* Our first attempt, using an optimistic padata list. */
        TRACE_INIT_CREDS_PREAUTH_OPTIMISTIC(context);
        code = k5_preauth(context, ctx, ctx->optimistic_padata, TRUE,
                          &ctx->request->padata, &ctx->selected_preauth_type);
        krb5_free_pa_data(context, ctx->optimistic_padata);
        ctx->optimistic_padata = NULL;
        if (code) {
            /* Make an unauthenticated request. */
            krb5_clear_error_message(context);
            code = 0;
        }
    } if (ctx->more_padata != NULL) {
        /* Continuing after KDC_ERR_MORE_PREAUTH_DATA_REQUIRED. */
        TRACE_INIT_CREDS_PREAUTH_MORE(context, ctx->selected_preauth_type);
        code = k5_preauth(context, ctx, ctx->more_padata, TRUE,
                          &ctx->request->padata, &pa_type);
    } else if (rcode == KDC_ERR_PREAUTH_FAILED) {
        /* Report the KDC-side failure code if we can't try another mech. */
        code = KRB5KDC_ERR_PREAUTH_FAILED;
    } else if (rcode && rcode != KDC_ERR_PREAUTH_REQUIRED) {
        /* Retrying after an error (possibly mechanism-specific), using error
         * padata to figure out what to change. */
        TRACE_INIT_CREDS_PREAUTH_TRYAGAIN(context, ctx->err_reply->error,
                                          ctx->selected_preauth_type);
        code = k5_preauth_tryagain(context, ctx, ctx->selected_preauth_type,
                                   ctx->err_reply, ctx->err_padata,
                                   &ctx->request->padata);
        if (code) {
            krb5_clear_error_message(context);
            code = ctx->err_reply->error + ERROR_TABLE_BASE_krb5;
        }
    }
    /* Don't continue after a keyboard interrupt. */
    if (code == KRB5_LIBOS_PWDINTR)
        goto cleanup;
    /* Don't continue if fallback is disabled. */
    if (code && ctx->fallback_disabled)
        goto cleanup;
    if (code) {
        /* See if we can try a different preauth mech before giving up. */
        k5_save_ctx_error(context, code, &save);
        ctx->selected_preauth_type = KRB5_PADATA_NONE;
    }

    if (ctx->request->padata == NULL && ctx->method_padata != NULL) {
        /* Retrying after KDC_ERR_PREAUTH_REQUIRED, or trying again with a
         * different mechanism after a failure. */
        TRACE_INIT_CREDS_PREAUTH(context);
        code = k5_preauth(context, ctx, ctx->method_padata, TRUE,
                          &ctx->request->padata, &ctx->selected_preauth_type);
        if (code) {
            if (save.code != 0)
                code = k5_restore_ctx_error(context, &save);
            goto cleanup;
        }
    }
    if (ctx->request->padata == NULL)
        TRACE_INIT_CREDS_PREAUTH_NONE(context);

    /* Remember when we sent this request (after any preauth delay). */
    ctx->request_time = time(NULL);

    if (ctx->encoded_previous_request != NULL) {
        krb5_free_data(context, ctx->encoded_previous_request);
        ctx->encoded_previous_request = NULL;
    }
    if (ctx->info_pa_permitted) {
        code = k5_add_empty_pa_data(&ctx->request->padata,
                                    KRB5_PADATA_AS_FRESHNESS);
        if (code)
            goto cleanup;
        code = k5_add_empty_pa_data(&ctx->request->padata,
                                    KRB5_ENCPADATA_REQ_ENC_PA_REP);
    }
    if (code)
        goto cleanup;

    if (ctx->subject_cert != NULL) {
        code = krb5int_copy_data_contents(context, ctx->subject_cert, &copy);
        if (code)
            goto cleanup;
        code = k5_add_pa_data_from_data(&ctx->request->padata,
                                        KRB5_PADATA_S4U_X509_USER, &copy);
        krb5_free_data_contents(context, &copy);
        if (code)
            goto cleanup;
    }

    code = maybe_add_pac_request(context, ctx);
    if (code)
        goto cleanup;

    code = krb5int_fast_prep_req(context, ctx->fast_state,
                                 ctx->request, ctx->outer_request_body,
                                 encode_krb5_as_req,
                                 &ctx->encoded_previous_request);
    if (code != 0)
        goto cleanup;

    code = krb5int_copy_data_contents(context,
                                      ctx->encoded_previous_request,
                                      out);
    if (code != 0)
        goto cleanup;

cleanup:
    krb5_free_pa_data(context, ctx->request->padata);
    ctx->request->padata = NULL;
    k5_clear_error(&save);
    return code;
}

/* Ensure that the reply enctype was among the requested enctypes. */
static krb5_error_code
check_reply_enctype(krb5_init_creds_context ctx)
{
    int i;

    for (i = 0; i < ctx->request->nktypes; i++) {
        if (ctx->request->ktype[i] == ctx->reply->enc_part.enctype)
            return 0;
    }
    return KRB5_CONFIG_ETYPE_NOSUPP;
}

/* Note the difference between the KDC's time, as reported to us in a
 * preauth-required error, and the current time. */
static void
note_req_timestamp(krb5_context context, krb5_init_creds_context ctx,
                   krb5_timestamp kdc_time, krb5_int32 kdc_usec)
{
    krb5_timestamp now;
    krb5_int32 usec;

    if (k5_time_with_offset(0, 0, &now, &usec) != 0)
        return;
    ctx->pa_offset = ts_delta(kdc_time, now);
    ctx->pa_offset_usec = kdc_usec - usec;
    ctx->pa_offset_state = (ctx->fast_state->armor_key != NULL) ?
        AUTH_OFFSET : UNAUTH_OFFSET;
}

/*
 * Determine whether err is a client referral to another realm, given the
 * previously requested client principal name.
 *
 * RFC 6806 Section 7 requires that KDCs return the referral realm in an error
 * type WRONG_REALM, but Microsoft Windows Server 2003 (and possibly others)
 * return the realm in a PRINCIPAL_UNKNOWN message.
 */
static krb5_boolean
is_referral(krb5_context context, krb5_error *err, krb5_principal client)
{
    if (err->error != KDC_ERR_WRONG_REALM &&
        err->error != KDC_ERR_C_PRINCIPAL_UNKNOWN)
        return FALSE;
    if (err->client == NULL)
        return FALSE;
    return !krb5_realm_compare(context, err->client, client);
}

/* Transfer error padata to method data in ctx and sort it according to
 * configuration. */
static krb5_error_code
accept_method_data(krb5_context context, krb5_init_creds_context ctx)
{
    krb5_free_pa_data(context, ctx->method_padata);
    ctx->method_padata = ctx->err_padata;
    ctx->err_padata = NULL;
    return sort_krb5_padata_sequence(context, &ctx->request->client->realm,
                                     ctx->method_padata);
}

/* Return the password expiry time indicated by enc_part2.  Set *is_last_req
 * if the information came from a last_req value. */
static void
get_expiry_times(krb5_enc_kdc_rep_part *enc_part2, krb5_timestamp *pw_exp,
                 krb5_timestamp *acct_exp, krb5_boolean *is_last_req)
{
    krb5_last_req_entry **last_req;
    krb5_int32 lr_type;

    *pw_exp = 0;
    *acct_exp = 0;
    *is_last_req = FALSE;

    /* Look for last-req entries for password or account expiration. */
    if (enc_part2->last_req) {
        for (last_req = enc_part2->last_req; *last_req; last_req++) {
            lr_type = (*last_req)->lr_type;
            if (lr_type == KRB5_LRQ_ALL_PW_EXPTIME ||
                lr_type == KRB5_LRQ_ONE_PW_EXPTIME) {
                *is_last_req = TRUE;
                *pw_exp = (*last_req)->value;
            } else if (lr_type == KRB5_LRQ_ALL_ACCT_EXPTIME ||
                       lr_type == KRB5_LRQ_ONE_ACCT_EXPTIME) {
                *is_last_req = TRUE;
                *acct_exp = (*last_req)->value;
            }
        }
    }

    /* If we didn't find any, use the ambiguous key_exp field. */
    if (*is_last_req == FALSE)
        *pw_exp = enc_part2->key_exp;
}

/*
 * Send an appropriate warning prompter if as_reply indicates that the password
 * is going to expire soon.  If an expire callback was provided, use that
 * instead.
 */
static void
warn_pw_expiry(krb5_context context, krb5_get_init_creds_opt *options,
               krb5_prompter_fct prompter, void *data,
               const char *in_tkt_service, krb5_kdc_rep *as_reply)
{
    krb5_error_code ret;
    krb5_expire_callback_func expire_cb;
    void *expire_data;
    krb5_timestamp pw_exp, acct_exp, now;
    krb5_boolean is_last_req;
    uint32_t interval;
    char ts[256], banner[1024];

    if (as_reply == NULL || as_reply->enc_part2 == NULL)
        return;

    get_expiry_times(as_reply->enc_part2, &pw_exp, &acct_exp, &is_last_req);

    k5_gic_opt_get_expire_cb(options, &expire_cb, &expire_data);
    if (expire_cb != NULL) {
        /* Invoke the expire callback and don't send prompter warnings. */
        (*expire_cb)(context, expire_data, pw_exp, acct_exp, is_last_req);
        return;
    }

    /* Don't warn if no password expiry value was sent. */
    if (pw_exp == 0)
        return;

    /* Don't warn if the password is being changed. */
    if (in_tkt_service && strcmp(in_tkt_service, "kadmin/changepw") == 0)
        return;

    /*
     * If the expiry time came from a last_req field, assume the KDC wants us
     * to warn.  Otherwise, warn only if the expiry time is less than a week
     * from now.
     */
    ret = krb5_timeofday(context, &now);
    if (ret != 0)
        return;
    interval = ts_interval(now, pw_exp);
    if (!is_last_req && (!interval || interval > 7 * 24 * 60 * 60))
        return;

    if (!prompter)
        return;

    ret = krb5_timestamp_to_string(pw_exp, ts, sizeof(ts));
    if (ret != 0)
        return;

    if (interval < 3600) {
        snprintf(banner, sizeof(banner),
                 _("Warning: Your password will expire in less than one hour "
                   "on %s"), ts);
    } else if (interval < 86400 * 2) {
        snprintf(banner, sizeof(banner),
                 _("Warning: Your password will expire in %d hour%s on %s"),
                 interval / 3600, interval < 7200 ? "" : "s", ts);
    } else {
        snprintf(banner, sizeof(banner),
                 _("Warning: Your password will expire in %d days on %s"),
                 interval / 86400, ts);
    }

    /* PROMPTER_INVOCATION */
    (*prompter)(context, data, 0, banner, 0, 0);
}

/* Display a warning via the prompter if a deprecated enctype was used for
 * either the reply key or the session key. */
static void
warn_deprecated(krb5_context context, krb5_init_creds_context ctx,
                krb5_enctype as_key_enctype)
{
    krb5_enctype etype;
    char encbuf[128], banner[256];

    if (ctx->prompter == NULL)
        return;

    if (krb5int_c_deprecated_enctype(as_key_enctype))
        etype = as_key_enctype;
    else if (krb5int_c_deprecated_enctype(ctx->cred.keyblock.enctype))
        etype = ctx->cred.keyblock.enctype;
    else
        return;

    if (krb5_enctype_to_name(etype, FALSE, encbuf, sizeof(encbuf)) != 0)
        return;
    snprintf(banner, sizeof(banner),
             _("Warning: encryption type %s used for authentication is "
               "deprecated and will be disabled"), encbuf);

    /* PROMPTER_INVOCATION */
    (*ctx->prompter)(context, ctx->prompter_data, NULL, banner, 0, NULL);
}

/*
 * If ctx specifies an output ccache, create or refresh it (atomically, if
 * possible) with the obtained credential and any appropriate ccache
 * configuration.
 */
static krb5_error_code
write_out_ccache(krb5_context context, krb5_init_creds_context ctx,
                 krb5_boolean fast_avail)
{
    krb5_error_code ret;
    krb5_ccache out_ccache = k5_gic_opt_get_out_ccache(ctx->opt);
    krb5_ccache mcc = NULL;
    krb5_data yes = string2data("yes");

    if (out_ccache == NULL)
        return 0;

    ret = krb5_cc_new_unique(context, "MEMORY", NULL, &mcc);
    if (ret)
        goto cleanup;

    ret = krb5_cc_initialize(context, mcc, ctx->cred.client);
    if (ret)
        goto cleanup;

    if (fast_avail) {
        ret = krb5_cc_set_config(context, mcc, ctx->cred.server,
                                 KRB5_CC_CONF_FAST_AVAIL, &yes);
        if (ret)
            goto cleanup;
    }

    ret = save_selected_preauth_type(context, mcc, ctx);
    if (ret)
        goto cleanup;

    ret = save_cc_config_out_data(context, mcc, ctx);
    if (ret)
        goto cleanup;

    ret = k5_cc_store_primary_cred(context, mcc, &ctx->cred);
    if (ret)
        goto cleanup;

    ret = krb5_cc_move(context, mcc, out_ccache);
    if (ret)
        goto cleanup;
    mcc = NULL;

cleanup:
    if (mcc != NULL)
        krb5_cc_destroy(context, mcc);
    return ret;
}

static krb5_error_code
init_creds_step_reply(krb5_context context,
                      krb5_init_creds_context ctx,
                      krb5_data *in)
{
    krb5_error_code code;
    krb5_pa_data **kdc_padata = NULL;
    krb5_preauthtype kdc_pa_type;
    krb5_boolean retry = FALSE;
    int canon_flag = 0;
    uint32_t reply_code;
    krb5_keyblock *strengthen_key = NULL;
    krb5_keyblock encrypting_key;
    krb5_boolean fast_avail;

    encrypting_key.length = 0;
    encrypting_key.contents = NULL;

    /* process previous KDC response */
    code = init_creds_validate_reply(context, ctx, in);
    if (code != 0)
        goto cleanup;

    /* per referrals draft, enterprise principals imply canonicalization */
    canon_flag = ((ctx->request->kdc_options & KDC_OPT_CANONICALIZE) != 0) ||
        ctx->request->client->type == KRB5_NT_ENTERPRISE_PRINCIPAL;

    if (ctx->err_reply != NULL) {
        krb5_free_pa_data(context, ctx->more_padata);
        krb5_free_pa_data(context, ctx->err_padata);
        ctx->more_padata = ctx->err_padata = NULL;
        code = krb5int_fast_process_error(context, ctx->fast_state,
                                          &ctx->err_reply, &ctx->err_padata,
                                          &retry);
        if (code != 0)
            goto cleanup;
        reply_code = ctx->err_reply->error;
        if (!ctx->restarted &&
            k5_upgrade_to_fast_p(context, ctx->fast_state, ctx->err_padata)) {
            /* Retry with FAST after discovering that the KDC supports
             * it.  (FAST negotiation usually avoids this restart.) */
            TRACE_FAST_PADATA_UPGRADE(context);
            ctx->restarted = TRUE;
            code = restart_init_creds_loop(context, ctx, TRUE);
        } else if (!ctx->restarted && reply_code == KDC_ERR_PREAUTH_FAILED &&
                   ctx->selected_preauth_type == KRB5_PADATA_NONE) {
            /* The KDC didn't like our informational padata (probably a pre-1.7
             * MIT krb5 KDC).  Retry without it. */
            ctx->info_pa_permitted = FALSE;
            ctx->restarted = TRUE;
            code = restart_init_creds_loop(context, ctx, FALSE);
        } else if (reply_code == KDC_ERR_PREAUTH_EXPIRED) {
            /* We sent an expired KDC cookie.  Start over, allowing another
             * FAST upgrade. */
            ctx->restarted = FALSE;
            code = restart_init_creds_loop(context, ctx, FALSE);
        } else if (ctx->identify_realm &&
                   (reply_code == KDC_ERR_PREAUTH_REQUIRED ||
                    reply_code == KDC_ERR_KEY_EXP)) {
            /* The client exists in this realm; we can stop. */
            ctx->complete = TRUE;
            goto cleanup;
        } else if (reply_code == KDC_ERR_PREAUTH_REQUIRED && retry) {
            note_req_timestamp(context, ctx, ctx->err_reply->stime,
                               ctx->err_reply->susec);
            code = accept_method_data(context, ctx);
        } else if (reply_code == KDC_ERR_PREAUTH_FAILED && retry) {
            note_req_timestamp(context, ctx, ctx->err_reply->stime,
                               ctx->err_reply->susec);
            /* Don't try again with the mechanism that failed. */
            code = k5_preauth_note_failed(ctx, ctx->selected_preauth_type);
            if (code)
                goto cleanup;
            ctx->selected_preauth_type = KRB5_PADATA_NONE;
            /* Accept or update method data if the KDC sent it. */
            if (ctx->err_padata != NULL)
                code = accept_method_data(context, ctx);
        } else if (reply_code == KDC_ERR_MORE_PREAUTH_DATA_REQUIRED && retry) {
            ctx->more_padata = ctx->err_padata;
            ctx->err_padata = NULL;
        } else if (canon_flag && is_referral(context, ctx->err_reply,
                                             ctx->request->client)) {
            TRACE_INIT_CREDS_REFERRAL(context, &ctx->err_reply->client->realm);
            /* Rewrite request.client with realm from error reply */
            krb5_free_data_contents(context, &ctx->request->client->realm);
            code = krb5int_copy_data_contents(context,
                                              &ctx->err_reply->client->realm,
                                              &ctx->request->client->realm);
            if (code != 0)
                goto cleanup;
            /* Reset per-realm negotiation state. */
            ctx->restarted = FALSE;
            ctx->info_pa_permitted = TRUE;
            code = restart_init_creds_loop(context, ctx, FALSE);
        } else {
            if (retry && ctx->selected_preauth_type != KRB5_PADATA_NONE) {
                code = 0;
            } else {
                /* error + no hints (or no preauth mech) = give up */
                code = (krb5_error_code)reply_code + ERROR_TABLE_BASE_krb5;
            }
        }

        /* Return error code, or continue with next iteration */
        goto cleanup;
    }

    /* We have a response. Process it. */
    assert(ctx->reply != NULL);

    /* Check for replies (likely forged) with unasked-for enctypes. */
    code = check_reply_enctype(ctx);
    if (code != 0)
        goto cleanup;

    /* process any preauth data in the as_reply */
    code = krb5int_fast_process_response(context, ctx->fast_state,
                                         ctx->reply, &strengthen_key);
    if (code != 0)
        goto cleanup;

    if (ctx->identify_realm) {
        /* Just getting a reply means the client exists in this realm. */
        ctx->complete = TRUE;
        goto cleanup;
    }

    code = sort_krb5_padata_sequence(context, &ctx->request->client->realm,
                                     ctx->reply->padata);
    if (code != 0)
        goto cleanup;

    ctx->etype = ctx->reply->enc_part.enctype;

    /* Process the final reply padata.  Don't restrict the preauth types or
     * record a selected preauth type. */
    ctx->allowed_preauth_type = KRB5_PADATA_NONE;
    code = k5_preauth(context, ctx, ctx->reply->padata, FALSE, &kdc_padata,
                      &kdc_pa_type);
    if (code != 0)
        goto cleanup;

    /*
     * If we haven't gotten a salt from another source yet, set up one
     * corresponding to the client principal returned by the KDC.  We
     * could get the same effect by passing local_as_reply->client to
     * gak_fct below, but that would put the canonicalized client name
     * in the prompt, which raises issues of needing to sanitize
     * unprintable characters.  So for now we just let it affect the
     * salt.  local_as_reply->client will be checked later on in
     * verify_as_reply.
     */
    if (ctx->default_salt) {
        code = krb5_principal2salt(context, ctx->reply->client, &ctx->salt);
        TRACE_INIT_CREDS_SALT_PRINC(context, &ctx->salt);
        if (code != 0)
            goto cleanup;
    }

    code = decrypt_as_reply(context, ctx, strengthen_key, &encrypting_key);
    if (code)
        goto cleanup;
    TRACE_INIT_CREDS_DECRYPTED_REPLY(context, ctx->reply->enc_part2->session);

    code = krb5int_fast_verify_nego(context, ctx->fast_state,
                                    ctx->reply, ctx->encoded_previous_request,
                                    &encrypting_key, &fast_avail);
    if (code)
        goto cleanup;
    code = verify_as_reply(context, ctx->request_time,
                           ctx->request, ctx->reply);
    if (code != 0)
        goto cleanup;
    code = verify_anonymous(context, ctx->request, ctx->reply,
                            &ctx->as_key);
    if (code)
        goto cleanup;

    code = stash_as_reply(context, ctx->reply, &ctx->cred, NULL);
    if (code != 0)
        goto cleanup;
    code = write_out_ccache(context, ctx, fast_avail);
    if (code)
        k5_prependmsg(context, code, _("Failed to store credentials"));

    k5_preauth_request_context_fini(context, ctx);

    /* success */
    ctx->complete = TRUE;
    warn_pw_expiry(context, ctx->opt, ctx->prompter, ctx->prompter_data,
                   ctx->in_tkt_service, ctx->reply);
    warn_deprecated(context, ctx, encrypting_key.enctype);

cleanup:
    krb5_free_pa_data(context, kdc_padata);
    krb5_free_keyblock(context, strengthen_key);
    krb5_free_keyblock_contents(context, &encrypting_key);

    return code;
}

/*
 * Do next step of credentials acquisition.
 *
 * On success returns 0 or KRB5KRB_ERR_RESPONSE_TOO_BIG if the request
 * should be sent with TCP.
 */
krb5_error_code KRB5_CALLCONV
krb5_init_creds_step(krb5_context context,
                     krb5_init_creds_context ctx,
                     krb5_data *in,
                     krb5_data *out,
                     krb5_data *realm,
                     unsigned int *flags)
{
    krb5_error_code code, code2;

    *flags = 0;

    out->data = NULL;
    out->length = 0;

    realm->data = NULL;
    realm->length = 0;

    if (ctx->complete)
        return EINVAL;

    code = k5_preauth_check_context(context, ctx);
    if (code)
        return code;

    if (in->length != 0) {
        code = init_creds_step_reply(context, ctx, in);
        if (code == KRB5KRB_ERR_RESPONSE_TOO_BIG) {
            code2 = krb5int_copy_data_contents(context,
                                               ctx->encoded_previous_request,
                                               out);
            if (code2 != 0) {
                code = code2;
                goto cleanup;
            }
            goto copy_realm;
        }
        if (code != 0 || ctx->complete)
            goto cleanup;
    } else {
        code = restart_init_creds_loop(context, ctx, FALSE);
        if (code)
            goto cleanup;
    }

    code = init_creds_step_request(context, ctx, out);
    if (code != 0)
        goto cleanup;

    /* Only a new request increments the loop count, not a TCP retry */
    ctx->loopcount++;

copy_realm:
    assert(ctx->request->server != NULL);

    code2 = krb5int_copy_data_contents(context,
                                       &ctx->request->server->realm,
                                       realm);
    if (code2 != 0) {
        code = code2;
        goto cleanup;
    }

cleanup:
    if (code == KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN) {
        char *client_name;

        /* See if we can produce a more detailed error message */
        code2 = krb5_unparse_name(context, ctx->request->client, &client_name);
        if (code2 == 0) {
            k5_setmsg(context, code,
                      _("Client '%s' not found in Kerberos database"),
                      client_name);
            krb5_free_unparsed_name(context, client_name);
        }
    }

    *flags = ctx->complete ? 0 : KRB5_INIT_CREDS_STEP_FLAG_CONTINUE;
    return code;
}

krb5_error_code KRB5_CALLCONV
k5_get_init_creds(krb5_context context, krb5_creds *creds,
                  krb5_principal client, krb5_prompter_fct prompter,
                  void *prompter_data, krb5_deltat start_time,
                  const char *in_tkt_service, krb5_get_init_creds_opt *options,
                  get_as_key_fn gak_fct, void *gak_data, int *use_primary,
                  krb5_kdc_rep **as_reply)
{
    krb5_error_code code;
    krb5_init_creds_context ctx = NULL;

    code = krb5_init_creds_init(context,
                                client,
                                prompter,
                                prompter_data,
                                start_time,
                                options,
                                &ctx);
    if (code != 0)
        goto cleanup;

    ctx->gak_fct = gak_fct;
    ctx->gak_data = gak_data;

    if (in_tkt_service) {
        code = krb5_init_creds_set_service(context, ctx, in_tkt_service);
        if (code != 0)
            goto cleanup;
    }

    code = k5_init_creds_get(context, ctx, use_primary);
    if (code != 0)
        goto cleanup;

    code = krb5_init_creds_get_creds(context, ctx, creds);
    if (code != 0)
        goto cleanup;

    if (as_reply != NULL) {
        *as_reply = ctx->reply;
        ctx->reply = NULL;
    }

cleanup:
    krb5_init_creds_free(context, ctx);

    return code;
}

krb5_error_code
k5_identify_realm(krb5_context context, krb5_principal client,
                  const krb5_data *subject_cert, krb5_principal *client_out)
{
    krb5_error_code ret;
    krb5_get_init_creds_opt *opts = NULL;
    krb5_init_creds_context ctx = NULL;
    int use_primary = 0;

    *client_out = NULL;

    ret = krb5_get_init_creds_opt_alloc(context, &opts);
    if (ret)
        goto cleanup;
    krb5_get_init_creds_opt_set_tkt_life(opts, 15);
    krb5_get_init_creds_opt_set_renew_life(opts, 0);
    krb5_get_init_creds_opt_set_forwardable(opts, 0);
    krb5_get_init_creds_opt_set_proxiable(opts, 0);
    krb5_get_init_creds_opt_set_canonicalize(opts, 1);

    ret = krb5_init_creds_init(context, client, NULL, NULL, 0, opts, &ctx);
    if (ret)
        goto cleanup;

    ctx->identify_realm = TRUE;
    ctx->subject_cert = subject_cert;

    ret = k5_init_creds_get(context, ctx, &use_primary);
    if (ret)
        goto cleanup;

    TRACE_INIT_CREDS_IDENTIFIED_REALM(context, &ctx->request->client->realm);
    ret = krb5_copy_principal(context, ctx->request->client, client_out);

cleanup:
    krb5_get_init_creds_opt_free(context, opts);
    krb5_init_creds_free(context, ctx);
    return ret;
}

krb5_error_code
k5_populate_gic_opt(krb5_context context, krb5_get_init_creds_opt **out,
                    krb5_flags options, krb5_address *const *addrs,
                    krb5_enctype *ktypes, krb5_preauthtype *pre_auth_types,
                    krb5_creds *creds)
{
    int i;
    krb5_timestamp starttime;
    krb5_deltat lifetime;
    krb5_get_init_creds_opt *opt;
    krb5_error_code retval;

    *out = NULL;
    retval = krb5_get_init_creds_opt_alloc(context, &opt);
    if (retval)
        return(retval);

    if (addrs)
        krb5_get_init_creds_opt_set_address_list(opt, (krb5_address **) addrs);
    if (ktypes) {
        i = k5_count_etypes(ktypes);
        if (i)
            krb5_get_init_creds_opt_set_etype_list(opt, ktypes, i);
    }
    if (pre_auth_types) {
        for (i=0; pre_auth_types[i]; i++);
        if (i)
            krb5_get_init_creds_opt_set_preauth_list(opt, pre_auth_types, i);
    }
    if (options&KDC_OPT_FORWARDABLE)
        krb5_get_init_creds_opt_set_forwardable(opt, 1);
    else krb5_get_init_creds_opt_set_forwardable(opt, 0);
    if (options&KDC_OPT_PROXIABLE)
        krb5_get_init_creds_opt_set_proxiable(opt, 1);
    else krb5_get_init_creds_opt_set_proxiable(opt, 0);
    if (creds && creds->times.endtime) {
        retval = krb5_timeofday(context, &starttime);
        if (retval)
            goto cleanup;
        if (creds->times.starttime) starttime = creds->times.starttime;
        lifetime = ts_delta(creds->times.endtime, starttime);
        krb5_get_init_creds_opt_set_tkt_life(opt, lifetime);
    }
    *out = opt;
    return 0;

cleanup:
    krb5_get_init_creds_opt_free(context, opt);
    return retval;
}
