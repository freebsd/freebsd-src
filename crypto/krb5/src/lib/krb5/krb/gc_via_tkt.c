/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/gc_via_tkt.c */
/*
 * Copyright 1990,1991,2007-2009 by the Massachusetts Institute of Technology.
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
 * Given a tkt, and a target cred, get it.
 * Assumes that the kdc_rep has been decrypted.
 */

#include "k5-int.h"
#include "int-proto.h"
#include "fast.h"

static krb5_error_code
kdcrep2creds(krb5_context context, krb5_kdc_rep *pkdcrep,
             krb5_address *const *address, krb5_boolean is_skey,
             krb5_data *psectkt, krb5_creds **ppcreds)
{
    krb5_error_code retval;
    krb5_data *pdata;

    if ((*ppcreds = (krb5_creds *)calloc(1,sizeof(krb5_creds))) == NULL) {
        return ENOMEM;
    }

    if ((retval = krb5_copy_principal(context, pkdcrep->client,
                                      &(*ppcreds)->client)))
        goto cleanup;

    if ((retval = krb5_copy_principal(context, pkdcrep->enc_part2->server,
                                      &(*ppcreds)->server)))
        goto cleanup;

    if ((retval = krb5_copy_keyblock_contents(context,
                                              pkdcrep->enc_part2->session,
                                              &(*ppcreds)->keyblock)))
        goto cleanup;
    TRACE_TGS_REPLY(context, (*ppcreds)->client, (*ppcreds)->server,
                    &(*ppcreds)->keyblock);

    if ((retval = krb5_copy_data(context, psectkt, &pdata)))
        goto cleanup_keyblock;
    (*ppcreds)->second_ticket = *pdata;
    free(pdata);

    (*ppcreds)->ticket_flags = pkdcrep->enc_part2->flags;
    (*ppcreds)->times = pkdcrep->enc_part2->times;
    (*ppcreds)->magic = KV5M_CREDS;

    (*ppcreds)->authdata = NULL;                        /* not used */
    (*ppcreds)->is_skey = is_skey;

    if (pkdcrep->enc_part2->caddrs) {
        if ((retval = krb5_copy_addresses(context, pkdcrep->enc_part2->caddrs,
                                          &(*ppcreds)->addresses)))
            goto cleanup_keyblock;
    } else {
        /* no addresses in the list means we got what we had */
        if ((retval = krb5_copy_addresses(context, address,
                                          &(*ppcreds)->addresses)))
            goto cleanup_keyblock;
    }

    if ((retval = encode_krb5_ticket(pkdcrep->ticket, &pdata)))
        goto cleanup_keyblock;

    (*ppcreds)->ticket = *pdata;
    free(pdata);
    return 0;

cleanup_keyblock:
    krb5_free_keyblock_contents(context, &(*ppcreds)->keyblock);

cleanup:
    free (*ppcreds);
    *ppcreds = NULL;
    return retval;
}

static krb5_error_code
check_reply_server(krb5_context context, krb5_flags kdcoptions,
                   krb5_creds *in_cred, krb5_kdc_rep *dec_rep)
{

    if (!krb5_principal_compare(context, dec_rep->ticket->server,
                                dec_rep->enc_part2->server))
        return KRB5_KDCREP_MODIFIED;

    /* Reply is self-consistent. */

    if (krb5_principal_compare(context, dec_rep->ticket->server,
                               in_cred->server))
        return 0;

    /* Server in reply differs from what we requested. */

    if (kdcoptions & KDC_OPT_CANONICALIZE) {
        /* in_cred server differs from ticket returned, but ticket
           returned is consistent and we requested canonicalization. */

        TRACE_CHECK_REPLY_SERVER_DIFFERS(context, in_cred->server,
                                         dec_rep->enc_part2->server);
        return 0;
    }

    /* We didn't request canonicalization. */

    if (!IS_TGS_PRINC(in_cred->server) ||
        !IS_TGS_PRINC(dec_rep->ticket->server)) {
        /* Canonicalization not requested, and not a TGS referral. */
        return KRB5_KDCREP_MODIFIED;
    }
    return 0;
}

/* Return true if a TGS credential is for the client's local realm. */
static inline int
tgt_is_local_realm(krb5_creds *tgt)
{
    return (tgt->server->length == 2
            && data_eq_string(tgt->server->data[0], KRB5_TGS_NAME)
            && data_eq(tgt->server->data[1], tgt->client->realm)
            && data_eq(tgt->server->realm, tgt->client->realm));
}

krb5_error_code
krb5_get_cred_via_tkt(krb5_context context, krb5_creds *tkt,
                      krb5_flags kdcoptions, krb5_address *const *address,
                      krb5_creds *in_cred, krb5_creds **out_cred)
{
    return krb5_get_cred_via_tkt_ext (context, tkt,
                                      kdcoptions, address,
                                      NULL, in_cred, NULL, NULL,
                                      NULL, NULL, out_cred, NULL);
}

krb5_error_code
krb5int_process_tgs_reply(krb5_context context,
                          struct krb5int_fast_request_state *fast_state,
                          krb5_data *response_data,
                          krb5_creds *tkt,
                          krb5_flags kdcoptions,
                          krb5_address *const *address,
                          krb5_pa_data **in_padata,
                          krb5_creds *in_cred,
                          krb5_timestamp timestamp,
                          krb5_int32 nonce,
                          krb5_keyblock *subkey,
                          krb5_pa_data ***out_padata,
                          krb5_pa_data ***out_enc_padata,
                          krb5_creds **out_cred)
{
    krb5_error_code retval;
    krb5_kdc_rep *dec_rep = NULL;
    krb5_error *err_reply = NULL;
    krb5_boolean s4u2self, is_skey;

    s4u2self = krb5int_find_pa_data(context, in_padata,
                                    KRB5_PADATA_S4U_X509_USER) ||
        krb5int_find_pa_data(context, in_padata,
                             KRB5_PADATA_FOR_USER);

    if (krb5_is_krb_error(response_data)) {
        retval = decode_krb5_error(response_data, &err_reply);
        if (retval != 0)
            goto cleanup;
        retval = krb5int_fast_process_error(context, fast_state,
                                            &err_reply, NULL, NULL);
        if (retval)
            goto cleanup;
        retval = (krb5_error_code) err_reply->error + ERROR_TABLE_BASE_krb5;
        if (err_reply->text.length > 0) {
            switch (err_reply->error) {
            case KRB_ERR_GENERIC:
                k5_setmsg(context, retval,
                          _("KDC returned error string: %.*s"),
                          err_reply->text.length, err_reply->text.data);
                break;
            case KDC_ERR_S_PRINCIPAL_UNKNOWN:
            {
                char *s_name;
                if (err_reply->server &&
                    krb5_unparse_name(context, err_reply->server, &s_name) == 0) {
                    k5_setmsg(context, retval,
                              _("Server %s not found in Kerberos database"),
                              s_name);
                    krb5_free_unparsed_name(context, s_name);
                } else
                    /* In case there's a stale S_PRINCIPAL_UNKNOWN
                       report already noted.  */
                    krb5_clear_error_message(context);
            }
            break;
            }
        }
        krb5_free_error(context, err_reply);
        goto cleanup;
    } else if (!krb5_is_tgs_rep(response_data)) {
        retval = KRB5KRB_AP_ERR_MSG_TYPE;
        goto cleanup;
    }

    /* Unfortunately, Heimdal at least up through 1.2  encrypts using
       the session key not the subsession key.  So we try both. */
    retval = krb5int_decode_tgs_rep(context, fast_state, response_data, subkey,
                                    KRB5_KEYUSAGE_TGS_REP_ENCPART_SUBKEY,
                                    &dec_rep);
    if (retval) {
        TRACE_TGS_REPLY_DECODE_SESSION(context, &tkt->keyblock);
        if ((krb5int_decode_tgs_rep(context, fast_state, response_data,
                                    &tkt->keyblock,
                                    KRB5_KEYUSAGE_TGS_REP_ENCPART_SESSKEY, &dec_rep)) == 0)
            retval = 0;
        else
            goto cleanup;
    }

    if (dec_rep->msg_type != KRB5_TGS_REP) {
        retval = KRB5KRB_AP_ERR_MSG_TYPE;
        goto cleanup;
    }

    /*
     * Don't trust the ok-as-delegate flag from foreign KDCs unless the
     * cross-realm TGT also had the ok-as-delegate flag set.
     */
    if (!tgt_is_local_realm(tkt)
        && !(tkt->ticket_flags & TKT_FLG_OK_AS_DELEGATE))
        dec_rep->enc_part2->flags &= ~TKT_FLG_OK_AS_DELEGATE;

    /* make sure the response hasn't been tampered with..... */
    retval = 0;

    if (s4u2self && !IS_TGS_PRINC(dec_rep->ticket->server)) {
        /* Final hop, check whether KDC supports S4U2Self */
        if (krb5_principal_compare(context, dec_rep->client, in_cred->server))
            retval = KRB5KDC_ERR_PADATA_TYPE_NOSUPP;
    } else if ((kdcoptions & KDC_OPT_CNAME_IN_ADDL_TKT) == 0 ||
               IS_TGS_PRINC(dec_rep->ticket->server)) {
        /*
         * For constrained delegation this check must be performed by caller,
         * as we can't decrypt the evidence ticket.  However, if it is a
         * referral the client should match the TGT client like normal.
         */
        if (!krb5_principal_compare(context, dec_rep->client, tkt->client))
            retval = KRB5_KDCREP_MODIFIED;
    }

    if (retval == 0)
        retval = check_reply_server(context, kdcoptions, in_cred, dec_rep);

    if (dec_rep->enc_part2->nonce != nonce)
        retval = KRB5_KDCREP_MODIFIED;

    if ((kdcoptions & KDC_OPT_POSTDATED) &&
        (in_cred->times.starttime != 0) &&
        (in_cred->times.starttime != dec_rep->enc_part2->times.starttime))
        retval = KRB5_KDCREP_MODIFIED;

    if ((in_cred->times.endtime != 0) &&
        ts_after(dec_rep->enc_part2->times.endtime, in_cred->times.endtime))
        retval = KRB5_KDCREP_MODIFIED;

    if ((kdcoptions & KDC_OPT_RENEWABLE) &&
        (in_cred->times.renew_till != 0) &&
        ts_after(dec_rep->enc_part2->times.renew_till,
                 in_cred->times.renew_till))
        retval = KRB5_KDCREP_MODIFIED;

    if ((kdcoptions & KDC_OPT_RENEWABLE_OK) &&
        (dec_rep->enc_part2->flags & KDC_OPT_RENEWABLE) &&
        (in_cred->times.endtime != 0) &&
        ts_after(dec_rep->enc_part2->times.renew_till, in_cred->times.endtime))
        retval = KRB5_KDCREP_MODIFIED;

    if (retval != 0)
        goto cleanup;

    if (!in_cred->times.starttime &&
        !ts_within(dec_rep->enc_part2->times.starttime, timestamp,
                   context->clockskew)) {
        retval = KRB5_KDCREP_SKEW;
        goto cleanup;
    }

    if (out_padata != NULL) {
        *out_padata = dec_rep->padata;
        dec_rep->padata = NULL;
    }
    if (out_enc_padata != NULL) {
        *out_enc_padata = dec_rep->enc_part2->enc_padata;
        dec_rep->enc_part2->enc_padata = NULL;
    }

    is_skey = (kdcoptions & KDC_OPT_ENC_TKT_IN_SKEY);
    retval = kdcrep2creds(context, dec_rep, address, is_skey,
                          &in_cred->second_ticket, out_cred);
    if (retval != 0)
        goto cleanup;

cleanup:
    if (dec_rep != NULL) {
        memset(dec_rep->enc_part2->session->contents, 0,
               dec_rep->enc_part2->session->length);
        krb5_free_kdc_rep(context, dec_rep);
    }

    return retval;
}

krb5_error_code
krb5_get_cred_via_tkt_ext(krb5_context context, krb5_creds *tkt,
                          krb5_flags kdcoptions, krb5_address *const *address,
                          krb5_pa_data **in_padata, krb5_creds *in_cred,
                          k5_pacb_fn pacb_fn, void *pacb_data,
                          krb5_pa_data ***out_padata,
                          krb5_pa_data ***out_enc_padata,
                          krb5_creds **out_cred, krb5_keyblock **out_subkey)
{
    krb5_error_code retval;
    krb5_data request_data;
    krb5_data response_data;
    krb5_timestamp timestamp;
    krb5_int32 nonce;
    krb5_keyblock *subkey = NULL;
    int tcp_only = 0, use_primary = 0;
    struct krb5int_fast_request_state *fast_state = NULL;

    request_data.data = NULL;
    request_data.length = 0;
    response_data.data = NULL;
    response_data.length = 0;

    retval = krb5int_fast_make_state(context, &fast_state);
    if (retval)
        goto cleanup;

    TRACE_GET_CRED_VIA_TKT_EXT(context, in_cred->server, tkt->server,
                               kdcoptions);

    retval = k5_make_tgs_req(context, fast_state, tkt, kdcoptions, address,
                             in_padata, in_cred, pacb_fn, pacb_data,
                             &request_data, &timestamp, &nonce, &subkey);
    if (retval != 0)
        goto cleanup;

send_again:
    use_primary = 0;
    retval = krb5_sendto_kdc(context, &request_data, &in_cred->server->realm,
                             &response_data, &use_primary, tcp_only);
    if (retval == 0) {
        if (krb5_is_krb_error(&response_data)) {
            if (!tcp_only) {
                krb5_error *err_reply;
                retval = decode_krb5_error(&response_data, &err_reply);
                if (retval != 0)
                    goto cleanup;
                retval = krb5int_fast_process_error(context, fast_state,
                                                    &err_reply, NULL, NULL);
                if (retval)
                    goto cleanup;
                if (err_reply->error == KRB_ERR_RESPONSE_TOO_BIG) {
                    tcp_only = 1;
                    krb5_free_error(context, err_reply);
                    krb5_free_data_contents(context, &response_data);
                    goto send_again;
                }
                krb5_free_error(context, err_reply);
            }
        }
    } else
        goto cleanup;

    retval = krb5int_process_tgs_reply(context, fast_state, &response_data,
                                       tkt, kdcoptions, address,
                                       in_padata, in_cred,
                                       timestamp, nonce, subkey,
                                       out_padata,
                                       out_enc_padata, out_cred);
    if (retval != 0)
        goto cleanup;

cleanup:
    krb5int_fast_free_state(context, fast_state);
    TRACE_GET_CRED_VIA_TKT_EXT_RETURN(context, retval);

    krb5_free_data_contents(context, &request_data);
    krb5_free_data_contents(context, &response_data);

    if (subkey != NULL) {
        if (retval == 0 && out_subkey != NULL)
            *out_subkey = subkey;
        else
            krb5_free_keyblock(context, subkey);
    }

    return retval;
}
