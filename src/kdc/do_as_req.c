/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/do_as_req.c */
/*
 * Portions Copyright (C) 2007 Apple Inc.
 * Copyright 1990, 1991, 2007, 2008, 2009, 2013, 2014 by the
 * Massachusetts Institute of Technology.  All Rights Reserved.
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
 *
 *
 * KDC Routines to deal with AS_REQ's
 */
/*
 * Copyright (c) 2006-2008, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "com_err.h"

#include <syslog.h>
#ifdef HAVE_NETINET_IN_H
#include <sys/types.h>
#include <netinet/in.h>
#ifndef hpux
#include <arpa/inet.h>
#endif  /* hpux */
#endif /* HAVE_NETINET_IN_H */

#include "kdc_util.h"
#include "kdc_audit.h"
#include "policy.h"
#include <kadm5/admin.h>
#include "adm_proto.h"
#include "extern.h"

static krb5_error_code
prepare_error_as(struct kdc_request_state *, krb5_kdc_req *, krb5_db_entry *,
                 int, krb5_pa_data **, krb5_boolean, krb5_principal,
                 krb5_data **, const char *);

/* Determine the key-expiration value according to RFC 4120 section 5.4.2. */
static krb5_timestamp
get_key_exp(krb5_db_entry *entry)
{
    if (entry->expiration == 0)
        return entry->pw_expiration;
    if (entry->pw_expiration == 0)
        return entry->expiration;
    return min(entry->expiration, entry->pw_expiration);
}

/*
 * Find the key in client for the most preferred enctype in req_enctypes.  Fill
 * in *kb_out with the decrypted keyblock (which the caller must free) and set
 * *kd_out to an alias to that key data entry.  Set *kd_out to NULL and leave
 * *kb_out zeroed if no key is found for any of the requested enctypes.
 * kb_out->enctype may differ from the enctype of *kd_out for DES enctypes; in
 * this case, kb_out->enctype is the requested enctype used to match the key
 * data entry.
 */
static krb5_error_code
select_client_key(krb5_context context, krb5_db_entry *client,
                  krb5_enctype *req_enctypes, int n_req_enctypes,
                  krb5_keyblock *kb_out, krb5_key_data **kd_out)
{
    krb5_error_code ret;
    krb5_key_data *kd;
    krb5_enctype etype;
    int i;

    memset(kb_out, 0, sizeof(*kb_out));
    *kd_out = NULL;

    for (i = 0; i < n_req_enctypes; i++) {
        etype = req_enctypes[i];
        if (!krb5_c_valid_enctype(etype))
            continue;
        if (krb5_dbe_find_enctype(context, client, etype, -1, 0, &kd) == 0) {
            /* Decrypt the client key data and set its enctype to the request
             * enctype (which may differ from the key data enctype for DES). */
            ret = krb5_dbe_decrypt_key_data(context, NULL, kd, kb_out, NULL);
            if (ret)
                return ret;
            kb_out->enctype = etype;
            *kd_out = kd;
            return 0;
        }
    }
    return 0;
}

struct as_req_state {
    loop_respond_fn respond;
    void *arg;

    krb5_principal_data client_princ;
    krb5_enc_tkt_part enc_tkt_reply;
    krb5_enc_kdc_rep_part reply_encpart;
    krb5_ticket ticket_reply;
    krb5_keyblock server_keyblock;
    krb5_keyblock client_keyblock;
    krb5_db_entry *client;
    krb5_db_entry *server;
    krb5_db_entry *local_tgt;
    krb5_db_entry *local_tgt_storage;
    krb5_key_data *client_key;
    krb5_kdc_req *request;
    struct krb5_kdcpreauth_rock_st rock;
    const char *status;
    krb5_pa_data **e_data;
    krb5_boolean typed_e_data;
    krb5_kdc_rep reply;
    krb5_timestamp kdc_time;
    krb5_timestamp authtime;
    krb5_keyblock session_key;
    unsigned int c_flags;
    krb5_data *req_pkt;
    krb5_data *inner_body;
    struct kdc_request_state *rstate;
    char *sname, *cname;
    void *pa_context;
    const krb5_fulladdr *from;
    krb5_data **auth_indicators;

    krb5_error_code preauth_err;

    kdc_realm_t *active_realm;
    krb5_audit_state *au_state;
};

static void
finish_process_as_req(struct as_req_state *state, krb5_error_code errcode)
{
    krb5_key_data *server_key;
    krb5_keyblock *as_encrypting_key = NULL;
    krb5_data *response = NULL;
    const char *emsg = 0;
    int did_log = 0;
    loop_respond_fn oldrespond;
    void *oldarg;
    kdc_realm_t *kdc_active_realm = state->active_realm;
    krb5_audit_state *au_state = state->au_state;

    assert(state);
    oldrespond = state->respond;
    oldarg = state->arg;

    if (errcode)
        goto egress;

    au_state->stage = ENCR_REP;

    if ((errcode = validate_forwardable(state->request, *state->client,
                                        *state->server, state->kdc_time,
                                        &state->status))) {
        errcode += ERROR_TABLE_BASE_krb5;
        goto egress;
    }

    errcode = check_indicators(kdc_context, state->server,
                               state->auth_indicators);
    if (errcode) {
        state->status = "HIGHER_AUTHENTICATION_REQUIRED";
        goto egress;
    }

    state->ticket_reply.enc_part2 = &state->enc_tkt_reply;

    /*
     * Find the server key
     */
    if ((errcode = krb5_dbe_find_enctype(kdc_context, state->server,
                                         -1, /* ignore keytype   */
                                         -1, /* Ignore salttype  */
                                         0,  /* Get highest kvno */
                                         &server_key))) {
        state->status = "FINDING_SERVER_KEY";
        goto egress;
    }

    /*
     * Convert server->key into a real key
     * (it may be encrypted in the database)
     *
     *  server_keyblock is later used to generate auth data signatures
     */
    if ((errcode = krb5_dbe_decrypt_key_data(kdc_context, NULL,
                                             server_key,
                                             &state->server_keyblock,
                                             NULL))) {
        state->status = "DECRYPT_SERVER_KEY";
        goto egress;
    }

    /* Start assembling the response */
    state->reply.msg_type = KRB5_AS_REP;
    state->reply.client = state->enc_tkt_reply.client; /* post canonization */
    state->reply.ticket = &state->ticket_reply;
    state->reply_encpart.session = &state->session_key;
    if ((errcode = fetch_last_req_info(state->client,
                                       &state->reply_encpart.last_req))) {
        state->status = "FETCH_LAST_REQ";
        goto egress;
    }
    state->reply_encpart.nonce = state->request->nonce;
    state->reply_encpart.key_exp = get_key_exp(state->client);
    state->reply_encpart.flags = state->enc_tkt_reply.flags;
    state->reply_encpart.server = state->ticket_reply.server;

    /* copy the time fields EXCEPT for authtime; it's location
     *  is used for ktime
     */
    state->reply_encpart.times = state->enc_tkt_reply.times;
    state->reply_encpart.times.authtime = state->authtime = state->kdc_time;

    state->reply_encpart.caddrs = state->enc_tkt_reply.caddrs;
    state->reply_encpart.enc_padata = NULL;

    /* Fetch the padata info to be returned (do this before
     *  authdata to handle possible replacement of reply key
     */
    errcode = return_padata(kdc_context, &state->rock, state->req_pkt,
                            state->request, &state->reply,
                            &state->client_keyblock, &state->pa_context);
    if (errcode) {
        state->status = "KDC_RETURN_PADATA";
        goto egress;
    }

    /* If we didn't find a client long-term key and no preauth mechanism
     * replaced the reply key, error out now. */
    if (state->client_keyblock.enctype == ENCTYPE_NULL) {
        state->status = "CANT_FIND_CLIENT_KEY";
        errcode = KRB5KDC_ERR_ETYPE_NOSUPP;
        goto egress;
    }

    errcode = handle_authdata(kdc_context,
                              state->c_flags,
                              state->client,
                              state->server,
                              NULL,
                              state->local_tgt,
                              &state->client_keyblock,
                              &state->server_keyblock,
                              NULL,
                              state->req_pkt,
                              state->request,
                              NULL, /* for_user_princ */
                              NULL, /* enc_tkt_request */
                              state->auth_indicators,
                              &state->enc_tkt_reply);
    if (errcode) {
        krb5_klog_syslog(LOG_INFO, _("AS_REQ : handle_authdata (%d)"),
                         errcode);
        state->status = "HANDLE_AUTHDATA";
        goto egress;
    }

    errcode = krb5_encrypt_tkt_part(kdc_context, &state->server_keyblock,
                                    &state->ticket_reply);
    if (errcode) {
        state->status = "ENCRYPT_TICKET";
        goto egress;
    }

    errcode = kau_make_tkt_id(kdc_context, &state->ticket_reply,
                              &au_state->tkt_out_id);
    if (errcode) {
        state->status = "GENERATE_TICKET_ID";
        goto egress;
    }

    state->ticket_reply.enc_part.kvno = server_key->key_data_kvno;
    errcode = kdc_fast_response_handle_padata(state->rstate,
                                              state->request,
                                              &state->reply,
                                              state->client_keyblock.enctype);
    if (errcode) {
        state->status = "MAKE_FAST_RESPONSE";
        goto egress;
    }

    /* now encode/encrypt the response */

    state->reply.enc_part.enctype = state->client_keyblock.enctype;

    errcode = kdc_fast_handle_reply_key(state->rstate, &state->client_keyblock,
                                        &as_encrypting_key);
    if (errcode) {
        state->status = "MAKE_FAST_REPLY_KEY";
        goto egress;
    }
    errcode = return_enc_padata(kdc_context, state->req_pkt, state->request,
                                as_encrypting_key, state->server,
                                &state->reply_encpart, FALSE);
    if (errcode) {
        state->status = "KDC_RETURN_ENC_PADATA";
        goto egress;
    }

    if (kdc_fast_hide_client(state->rstate))
        state->reply.client = (krb5_principal)krb5_anonymous_principal();
    errcode = krb5_encode_kdc_rep(kdc_context, KRB5_AS_REP,
                                  &state->reply_encpart, 0,
                                  as_encrypting_key,
                                  &state->reply, &response);
    if (state->client_key != NULL)
        state->reply.enc_part.kvno = state->client_key->key_data_kvno;
    if (errcode) {
        state->status = "ENCODE_KDC_REP";
        goto egress;
    }

    /* these parts are left on as a courtesy from krb5_encode_kdc_rep so we
       can use them in raw form if needed.  But, we don't... */
    memset(state->reply.enc_part.ciphertext.data, 0,
           state->reply.enc_part.ciphertext.length);
    free(state->reply.enc_part.ciphertext.data);

    log_as_req(kdc_context, state->from, state->request, &state->reply,
               state->client, state->cname, state->server,
               state->sname, state->authtime, 0, 0, 0);
    did_log = 1;

egress:
    if (errcode != 0)
        assert (state->status != 0);

    au_state->status = state->status;
    au_state->reply = &state->reply;
    kau_as_req(kdc_context,
              (errcode || state->preauth_err) ? FALSE : TRUE, au_state);
    kau_free_kdc_req(au_state);

    free_padata_context(kdc_context, state->pa_context);
    if (as_encrypting_key)
        krb5_free_keyblock(kdc_context, as_encrypting_key);
    if (errcode)
        emsg = krb5_get_error_message(kdc_context, errcode);

    if (state->status) {
        log_as_req(kdc_context,
                   state->from, state->request, &state->reply, state->client,
                   state->cname, state->server, state->sname, state->authtime,
                   state->status, errcode, emsg);
        did_log = 1;
    }
    if (errcode) {
        if (state->status == 0) {
            state->status = emsg;
        }
        if (errcode != KRB5KDC_ERR_DISCARD) {
            errcode -= ERROR_TABLE_BASE_krb5;
            if (errcode < 0 || errcode > KRB_ERR_MAX)
                errcode = KRB_ERR_GENERIC;

            errcode = prepare_error_as(state->rstate, state->request,
                                       state->local_tgt, errcode,
                                       state->e_data, state->typed_e_data,
                                       ((state->client != NULL) ?
                                        state->client->princ : NULL),
                                       &response, state->status);
            state->status = 0;
        }
    }

    if (emsg)
        krb5_free_error_message(kdc_context, emsg);
    if (state->enc_tkt_reply.authorization_data != NULL)
        krb5_free_authdata(kdc_context,
                           state->enc_tkt_reply.authorization_data);
    if (state->server_keyblock.contents != NULL)
        krb5_free_keyblock_contents(kdc_context, &state->server_keyblock);
    if (state->client_keyblock.contents != NULL)
        krb5_free_keyblock_contents(kdc_context, &state->client_keyblock);
    if (state->reply.padata != NULL)
        krb5_free_pa_data(kdc_context, state->reply.padata);
    if (state->reply_encpart.enc_padata)
        krb5_free_pa_data(kdc_context, state->reply_encpart.enc_padata);

    if (state->cname != NULL)
        free(state->cname);
    if (state->sname != NULL)
        free(state->sname);
    krb5_db_free_principal(kdc_context, state->client);
    krb5_db_free_principal(kdc_context, state->server);
    krb5_db_free_principal(kdc_context, state->local_tgt_storage);
    if (state->session_key.contents != NULL)
        krb5_free_keyblock_contents(kdc_context, &state->session_key);
    if (state->ticket_reply.enc_part.ciphertext.data != NULL) {
        memset(state->ticket_reply.enc_part.ciphertext.data , 0,
               state->ticket_reply.enc_part.ciphertext.length);
        free(state->ticket_reply.enc_part.ciphertext.data);
    }

    krb5_free_pa_data(kdc_context, state->e_data);
    krb5_free_data(kdc_context, state->inner_body);
    kdc_free_rstate(state->rstate);
    krb5_free_kdc_req(kdc_context, state->request);
    k5_free_data_ptr_list(state->auth_indicators);
    assert(did_log != 0);

    free(state);
    (*oldrespond)(oldarg, errcode, response);
}

static void
finish_missing_required_preauth(void *arg)
{
    struct as_req_state *state = (struct as_req_state *)arg;

    finish_process_as_req(state, state->preauth_err);
}

static void
finish_preauth(void *arg, krb5_error_code code)
{
    struct as_req_state *state = arg;
    krb5_error_code real_code = code;

    if (code) {
        if (vague_errors)
            code = KRB5KRB_ERR_GENERIC;
        state->status = "PREAUTH_FAILED";
        if (real_code == KRB5KDC_ERR_PREAUTH_FAILED) {
            state->preauth_err = code;
            get_preauth_hint_list(state->request, &state->rock, &state->e_data,
                                  finish_missing_required_preauth, state);
            return;
        }
    } else {
        /*
         * Final check before handing out ticket: If the client requires
         * preauthentication, verify that the proper kind of
         * preauthentication was carried out.
         */
        state->status = missing_required_preauth(state->client, state->server,
                                                 &state->enc_tkt_reply);
        if (state->status) {
            state->preauth_err = KRB5KDC_ERR_PREAUTH_REQUIRED;
            get_preauth_hint_list(state->request, &state->rock, &state->e_data,
                                  finish_missing_required_preauth, state);
            return;
        }
    }

    finish_process_as_req(state, code);
}

/*ARGSUSED*/
void
process_as_req(krb5_kdc_req *request, krb5_data *req_pkt,
               const krb5_fulladdr *from, kdc_realm_t *kdc_active_realm,
               verto_ctx *vctx, loop_respond_fn respond, void *arg)
{
    krb5_error_code errcode;
    unsigned int s_flags = 0;
    krb5_data encoded_req_body;
    krb5_enctype useenctype;
    struct as_req_state *state;
    krb5_audit_state *au_state = NULL;

    state = k5alloc(sizeof(*state), &errcode);
    if (state == NULL) {
        (*respond)(arg, errcode, NULL);
        return;
    }
    state->respond = respond;
    state->arg = arg;
    state->request = request;
    state->req_pkt = req_pkt;
    state->from = from;
    state->active_realm = kdc_active_realm;

    errcode = kdc_make_rstate(kdc_active_realm, &state->rstate);
    if (errcode != 0) {
        (*respond)(arg, errcode, NULL);
        free(state);
        return;
    }

    /* Initialize audit state. */
    errcode = kau_init_kdc_req(kdc_context, state->request, from, &au_state);
    if (errcode) {
        (*respond)(arg, errcode, NULL);
        kdc_free_rstate(state->rstate);
        free(state);
        return;
    }
    state->au_state = au_state;

    if (state->request->msg_type != KRB5_AS_REQ) {
        state->status = "VALIDATE_MESSAGE_TYPE";
        errcode = KRB5_BADMSGTYPE;
        goto errout;
    }

    /* Seed the audit trail with the request ID and basic information. */
    kau_as_req(kdc_context, TRUE, au_state);

    if (fetch_asn1_field((unsigned char *) req_pkt->data,
                         1, 4, &encoded_req_body) != 0) {
        errcode = ASN1_BAD_ID;
        state->status = "FETCH_REQ_BODY";
        goto errout;
    }
    errcode = kdc_find_fast(&state->request, &encoded_req_body, NULL, NULL,
                            state->rstate, &state->inner_body);
    if (errcode) {
        state->status = "FIND_FAST";
        goto errout;
    }
    if (state->inner_body == NULL) {
        /* Not a FAST request; copy the encoded request body. */
        errcode = krb5_copy_data(kdc_context, &encoded_req_body,
                                 &state->inner_body);
        if (errcode) {
            state->status = "COPY_REQ_BODY";
            goto errout;
        }
    }
    au_state->request = state->request;
    state->rock.request = state->request;
    state->rock.inner_body = state->inner_body;
    state->rock.rstate = state->rstate;
    state->rock.vctx = vctx;
    state->rock.auth_indicators = &state->auth_indicators;
    if (!state->request->client) {
        state->status = "NULL_CLIENT";
        errcode = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
        goto errout;
    }
    if ((errcode = krb5_unparse_name(kdc_context,
                                     state->request->client,
                                     &state->cname))) {
        state->status = "UNPARSE_CLIENT";
        goto errout;
    }
    limit_string(state->cname);

    if (!state->request->server) {
        state->status = "NULL_SERVER";
        errcode = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
        goto errout;
    }
    if ((errcode = krb5_unparse_name(kdc_context,
                                     state->request->server,
                                     &state->sname))) {
        state->status = "UNPARSE_SERVER";
        goto errout;
    }
    limit_string(state->sname);

    /*
     * We set KRB5_KDB_FLAG_CLIENT_REFERRALS_ONLY as a hint
     * to the backend to return naming information in lieu
     * of cross realm TGS entries.
     */
    setflag(state->c_flags, KRB5_KDB_FLAG_CLIENT_REFERRALS_ONLY);
    /*
     * Note that according to the referrals draft we should
     * always canonicalize enterprise principal names.
     */
    if (isflagset(state->request->kdc_options, KDC_OPT_CANONICALIZE) ||
        state->request->client->type == KRB5_NT_ENTERPRISE_PRINCIPAL) {
        setflag(state->c_flags, KRB5_KDB_FLAG_CANONICALIZE);
        setflag(state->c_flags, KRB5_KDB_FLAG_ALIAS_OK);
    }
    if (include_pac_p(kdc_context, state->request)) {
        setflag(state->c_flags, KRB5_KDB_FLAG_INCLUDE_PAC);
    }
    errcode = krb5_db_get_principal(kdc_context, state->request->client,
                                    state->c_flags, &state->client);
    if (errcode == KRB5_KDB_CANTLOCK_DB)
        errcode = KRB5KDC_ERR_SVC_UNAVAILABLE;
    if (errcode == KRB5_KDB_NOENTRY) {
        state->status = "CLIENT_NOT_FOUND";
        if (vague_errors)
            errcode = KRB5KRB_ERR_GENERIC;
        else
            errcode = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
        goto errout;
    } else if (errcode) {
        state->status = "LOOKING_UP_CLIENT";
        goto errout;
    }
    state->rock.client = state->client;

    /*
     * If the backend returned a principal that is not in the local
     * realm, then we need to refer the client to that realm.
     */
    if (!is_local_principal(kdc_active_realm, state->client->princ)) {
        /* Entry is a referral to another realm */
        state->status = "REFERRAL";
        au_state->cl_realm = &state->client->princ->realm;
        errcode = KRB5KDC_ERR_WRONG_REALM;
        goto errout;
    }

    au_state->stage = SRVC_PRINC;

    s_flags = 0;
    setflag(s_flags, KRB5_KDB_FLAG_ALIAS_OK);
    if (isflagset(state->request->kdc_options, KDC_OPT_CANONICALIZE)) {
        setflag(s_flags, KRB5_KDB_FLAG_CANONICALIZE);
    }
    errcode = krb5_db_get_principal(kdc_context, state->request->server,
                                    s_flags, &state->server);
    if (errcode == KRB5_KDB_CANTLOCK_DB)
        errcode = KRB5KDC_ERR_SVC_UNAVAILABLE;
    if (errcode == KRB5_KDB_NOENTRY) {
        state->status = "SERVER_NOT_FOUND";
        errcode = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
        goto errout;
    } else if (errcode) {
        state->status = "LOOKING_UP_SERVER";
        goto errout;
    }

    errcode = get_local_tgt(kdc_context, &state->request->server->realm,
                            state->server, &state->local_tgt,
                            &state->local_tgt_storage);
    if (errcode) {
        state->status = "GET_LOCAL_TGT";
        goto errout;
    }

    au_state->stage = VALIDATE_POL;

    if ((errcode = krb5_timeofday(kdc_context, &state->kdc_time))) {
        state->status = "TIMEOFDAY";
        goto errout;
    }
    state->authtime = state->kdc_time; /* for audit_as_request() */

    if ((errcode = validate_as_request(kdc_active_realm,
                                       state->request, *state->client,
                                       *state->server, state->kdc_time,
                                       &state->status, &state->e_data))) {
        if (!state->status)
            state->status = "UNKNOWN_REASON";
        errcode += ERROR_TABLE_BASE_krb5;
        goto errout;
    }

    au_state->stage = ISSUE_TKT;

    /*
     * Select the keytype for the ticket session key.
     */
    if ((useenctype = select_session_keytype(kdc_active_realm, state->server,
                                             state->request->nktypes,
                                             state->request->ktype)) == 0) {
        /* unsupported ktype */
        state->status = "BAD_ENCRYPTION_TYPE";
        errcode = KRB5KDC_ERR_ETYPE_NOSUPP;
        goto errout;
    }

    if ((errcode = krb5_c_make_random_key(kdc_context, useenctype,
                                          &state->session_key))) {
        state->status = "MAKE_RANDOM_KEY";
        goto errout;
    }

    /*
     * Canonicalization is only effective if we are issuing a TGT
     * (the intention is to allow support for Windows "short" realm
     * aliases, nothing more).
     */
    if (isflagset(s_flags, KRB5_KDB_FLAG_CANONICALIZE) &&
        krb5_is_tgs_principal(state->request->server) &&
        krb5_is_tgs_principal(state->server->princ)) {
        state->ticket_reply.server = state->server->princ;
    } else {
        state->ticket_reply.server = state->request->server;
    }

    /* Copy options that request the corresponding ticket flags. */
    state->enc_tkt_reply.flags = OPTS2FLAGS(state->request->kdc_options);
    state->enc_tkt_reply.times.authtime = state->authtime;

    setflag(state->enc_tkt_reply.flags, TKT_FLG_INITIAL);
    setflag(state->enc_tkt_reply.flags, TKT_FLG_ENC_PA_REP);

    /*
     * It should be noted that local policy may affect the
     * processing of any of these flags.  For example, some
     * realms may refuse to issue renewable tickets
     */

    state->enc_tkt_reply.session = &state->session_key;
    if (isflagset(state->c_flags, KRB5_KDB_FLAG_CANONICALIZE)) {
        state->client_princ = *(state->client->princ);
    } else {
        state->client_princ = *(state->request->client);
        /* The realm is always canonicalized */
        state->client_princ.realm = state->client->princ->realm;
    }
    state->enc_tkt_reply.client = &state->client_princ;
    state->enc_tkt_reply.transited.tr_type = KRB5_DOMAIN_X500_COMPRESS;
    state->enc_tkt_reply.transited.tr_contents = empty_string;

    if (isflagset(state->request->kdc_options, KDC_OPT_POSTDATED)) {
        setflag(state->enc_tkt_reply.flags, TKT_FLG_INVALID);
        state->enc_tkt_reply.times.starttime = state->request->from;
    } else
        state->enc_tkt_reply.times.starttime = state->kdc_time;

    kdc_get_ticket_endtime(kdc_active_realm,
                           state->enc_tkt_reply.times.starttime,
                           kdc_infinity, state->request->till, state->client,
                           state->server, &state->enc_tkt_reply.times.endtime);

    kdc_get_ticket_renewtime(kdc_active_realm, state->request, NULL,
                             state->client, state->server,
                             &state->enc_tkt_reply);

    /*
     * starttime is optional, and treated as authtime if not present.
     * so we can nuke it if it matches
     */
    if (state->enc_tkt_reply.times.starttime ==
        state->enc_tkt_reply.times.authtime)
        state->enc_tkt_reply.times.starttime = 0;

    state->enc_tkt_reply.caddrs = state->request->addresses;
    state->enc_tkt_reply.authorization_data = 0;

    /* If anonymous requests are being used, adjust the realm of the client
     * principal. */
    if (isflagset(state->request->kdc_options, KDC_OPT_REQUEST_ANONYMOUS)) {
        if (!krb5_principal_compare_any_realm(kdc_context,
                                              state->request->client,
                                              krb5_anonymous_principal())) {
            errcode = KRB5KDC_ERR_BADOPTION;
            /* Anonymous requested but anonymous principal not used.*/
            state->status = "VALIDATE_ANONYMOUS_PRINCIPAL";
            goto errout;
        }
        krb5_free_principal(kdc_context, state->request->client);
        state->request->client = NULL;
        errcode = krb5_copy_principal(kdc_context, krb5_anonymous_principal(),
                                      &state->request->client);
        if (errcode) {
            state->status = "COPY_ANONYMOUS_PRINCIPAL";
            goto errout;
        }
        state->enc_tkt_reply.client = state->request->client;
        setflag(state->client->attributes, KRB5_KDB_REQUIRES_PRE_AUTH);
    }

    errcode = select_client_key(kdc_context, state->client,
                                state->request->ktype, state->request->nktypes,
                                &state->client_keyblock, &state->client_key);
    if (errcode) {
        state->status = "DECRYPT_CLIENT_KEY";
        goto errout;
    }
    if (state->client_key != NULL) {
        state->rock.client_key = state->client_key;
        state->rock.client_keyblock = &state->client_keyblock;
    }

    errcode = kdc_fast_read_cookie(kdc_context, state->rstate, state->request,
                                   state->local_tgt);
    if (errcode) {
        state->status = "READ_COOKIE";
        goto errout;
    }

    /*
     * Check the preauthentication if it is there.
     */
    if (state->request->padata) {
        check_padata(kdc_context, &state->rock, state->req_pkt,
                     state->request, &state->enc_tkt_reply, &state->pa_context,
                     &state->e_data, &state->typed_e_data, finish_preauth,
                     state);
    } else
        finish_preauth(state, 0);
    return;

errout:
    finish_process_as_req(state, errcode);
}

static krb5_error_code
prepare_error_as(struct kdc_request_state *rstate, krb5_kdc_req *request,
                 krb5_db_entry *local_tgt, int error, krb5_pa_data **e_data_in,
                 krb5_boolean typed_e_data, krb5_principal canon_client,
                 krb5_data **response, const char *status)
{
    krb5_error errpkt;
    krb5_error_code retval;
    krb5_data *scratch = NULL, *e_data_asn1 = NULL, *fast_edata = NULL;
    krb5_pa_data **e_data = NULL, *cookie = NULL;
    kdc_realm_t *kdc_active_realm = rstate->realm_data;
    size_t count;

    if (e_data_in != NULL) {
        /* Add a PA-FX-COOKIE to e_data_in.  e_data is a shallow copy
         * containing aliases. */
        for (count = 0; e_data_in[count] != NULL; count++);
        e_data = calloc(count + 2, sizeof(*e_data));
        if (e_data == NULL)
            return ENOMEM;
        memcpy(e_data, e_data_in, count * sizeof(*e_data));
        retval = kdc_fast_make_cookie(kdc_context, rstate, local_tgt,
                                      request->client, &cookie);
        e_data[count] = cookie;
    }

    errpkt.ctime = request->nonce;
    errpkt.cusec = 0;

    retval = krb5_us_timeofday(kdc_context, &errpkt.stime, &errpkt.susec);
    if (retval)
        goto cleanup;
    errpkt.error = error;
    errpkt.server = request->server;
    errpkt.client = (error == KDC_ERR_WRONG_REALM) ? canon_client :
        request->client;
    errpkt.text = string2data((char *)status);

    if (e_data != NULL) {
        if (typed_e_data)
            retval = encode_krb5_typed_data(e_data, &e_data_asn1);
        else
            retval = encode_krb5_padata_sequence(e_data, &e_data_asn1);
        if (retval)
            goto cleanup;
        errpkt.e_data = *e_data_asn1;
    } else
        errpkt.e_data = empty_data();

    retval = kdc_fast_handle_error(kdc_context, rstate, request, e_data,
                                   &errpkt, &fast_edata);
    if (retval)
        goto cleanup;
    if (fast_edata != NULL)
        errpkt.e_data = *fast_edata;

    scratch = k5alloc(sizeof(*scratch), &retval);
    if (scratch == NULL)
        goto cleanup;
    if (kdc_fast_hide_client(rstate) && errpkt.client != NULL)
        errpkt.client = (krb5_principal)krb5_anonymous_principal();
    retval = krb5_mk_error(kdc_context, &errpkt, scratch);
    if (retval)
        goto cleanup;

    *response = scratch;
    scratch = NULL;

cleanup:
    krb5_free_data(kdc_context, fast_edata);
    krb5_free_data(kdc_context, e_data_asn1);
    free(scratch);
    free(e_data);
    if (cookie != NULL)
        free(cookie->contents);
    free(cookie);
    return retval;
}
