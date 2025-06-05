/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/do_tgs_req.c - KDC Routines to deal with TGS_REQ's */
/*
 * Copyright 1990, 1991, 2001, 2007, 2008, 2009, 2013, 2014 by the
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

#include <syslog.h>
#ifdef HAVE_NETINET_IN_H
#include <sys/types.h>
#include <netinet/in.h>
#ifndef hpux
#include <arpa/inet.h>
#endif
#endif

#include "kdc_util.h"
#include "kdc_audit.h"
#include "policy.h"
#include "extern.h"
#include "adm_proto.h"
#include <ctype.h>

struct tgs_req_info {
    /* The decoded request.  Ownership is transferred to this structure.  This
     * will be replaced with the inner FAST body if present. */
    krb5_kdc_req *req;

    /*
     * The decrypted authentication header ticket from the request's
     * PA-TGS-REQ, the KDB entry for its server, its encryption key, the
     * PA-TGS-REQ subkey if present, and the decoded and verified header ticket
     * PAC if present.
     */
    krb5_ticket *header_tkt;
    krb5_db_entry *header_server;
    krb5_keyblock *header_key;
    krb5_keyblock *subkey;
    krb5_pac header_pac;

    /*
     * If a second ticket is present and this is a U2U or S4U2Proxy request,
     * the decoded and verified PAC if present, the KDB entry for the second
     * ticket server server, and the key used to decrypt the second ticket.
     */
    krb5_pac stkt_pac;
    krb5_db_entry *stkt_server;
    krb5_keyblock *stkt_server_key;
    /* For cross-realm S4U2Proxy requests, the client principal retrieved from
     * stkt_pac. */
    krb5_principal stkt_pac_client;

    /* Storage for the local TGT KDB entry for the service realm if that isn't
     * the header server. */
    krb5_db_entry *local_tgt_storage;
    /* The decrypted first key of the local TGT entry. */
    krb5_keyblock local_tgt_key;

    /* The server KDB entry.  Normally the requested server, but for referral
     * and alternate TGS replies this will be a cross-realm TGT entry. */
    krb5_db_entry *server;

    /*
     * The subject client KDB entry for an S4U2Self request, or the header
     * ticket client KDB entry for other requests.  NULL if
     * NO_AUTH_DATA_REQUIRED is set on the server KDB entry and this isn't an
     * S4U2Self request, or if the client is in another realm and the KDB
     * cannot map its principal name.
     */
    krb5_db_entry *client;

    /* The decoded S4U2Self padata from the request, if present. */
    krb5_pa_s4u_x509_user *s4u2self;

    /* Authentication indicators retrieved from the header ticket, for
     * non-S4U2Self requests. */
    krb5_data **auth_indicators;

    /* Storage for a transited list with the header TGT realm added, if that
     * realm is different from the client and server realm. */
    krb5_data new_transited;

    /* The KDB flags applicable to this request (a subset of {CROSS_REALM,
     * ISSUING_REFERRAL, PROTOCOL_TRANSITION, CONSTRAINED_DELEGATION}). */
    unsigned int flags;

    /* Booleans for two of the above flags, for convenience. */
    krb5_boolean is_referral;
    krb5_boolean is_crossrealm;

    /* The authtime of subject_tkt.  On early failures this may be 0. */
    krb5_timestamp authtime;

    /* The following fields are (or contain) alias pointers and should not be
     * freed. */

    /* The transited list implied by the request, aliasing new_transited or the
     * header ticket transited field. */
    krb5_transited transited;

    /* Alias to the decrypted second ticket within req, if one applies to this
     * request. */
    const krb5_ticket *stkt;

    /* Alias to stkt for S4U2Proxy requests, header_tkt otherwise. */
    krb5_enc_tkt_part *subject_tkt;

    /* Alias to local_tgt_storage or header_server. */
    krb5_db_entry *local_tgt;

    /* For either kind of S4U request, an alias to the requested client
     * principal name. */
    krb5_principal s4u_cprinc;

    /* An alias to the client principal name we should issue the ticket for
     * (either header_tkt->enc_part2->client or s4u_cprinc). */
    krb5_principal tkt_client;

    /* The client principal of the PA-TGS-REQ header ticket.  On early failures
     * this may be NULL. */
    krb5_principal cprinc;

    /* The canonicalized request server principal or referral/alternate TGT.
     * On early failures this may be the requested server instead. */
    krb5_principal sprinc;

};

static krb5_error_code
db_get_svc_princ(krb5_context, krb5_principal, krb5_flags,
                 krb5_db_entry **, const char **);

static krb5_error_code
prepare_error_tgs(struct kdc_request_state *state, krb5_kdc_req *request,
                  krb5_ticket *ticket, krb5_error_code code,
                  krb5_principal canon_server, krb5_data **response,
                  const char *status, krb5_pa_data **e_data)
{
    krb5_context context = state->realm_data->realm_context;
    krb5_error errpkt;
    krb5_error_code retval = 0;
    krb5_data *scratch, *e_data_asn1 = NULL, *fast_edata = NULL;

    errpkt.magic = KV5M_ERROR;
    errpkt.ctime = 0;
    errpkt.cusec = 0;

    retval = krb5_us_timeofday(context, &errpkt.stime, &errpkt.susec);
    if (retval)
        return(retval);
    errpkt.error = errcode_to_protocol(code);
    errpkt.server = request->server;
    if (ticket && ticket->enc_part2)
        errpkt.client = ticket->enc_part2->client;
    else
        errpkt.client = NULL;
    errpkt.text.length = strlen(status);
    if (!(errpkt.text.data = strdup(status)))
        return ENOMEM;

    if (!(scratch = (krb5_data *)malloc(sizeof(*scratch)))) {
        free(errpkt.text.data);
        return ENOMEM;
    }

    if (e_data != NULL) {
        retval = encode_krb5_padata_sequence(e_data, &e_data_asn1);
        if (retval) {
            free(scratch);
            free(errpkt.text.data);
            return retval;
        }
        errpkt.e_data = *e_data_asn1;
    } else
        errpkt.e_data = empty_data();

    retval = kdc_fast_handle_error(context, state, request, e_data,
                                   &errpkt, &fast_edata);
    if (retval) {
        free(scratch);
        free(errpkt.text.data);
        krb5_free_data(context, e_data_asn1);
        return retval;
    }
    if (fast_edata)
        errpkt.e_data = *fast_edata;
    if (kdc_fast_hide_client(state) && errpkt.client != NULL)
        errpkt.client = (krb5_principal)krb5_anonymous_principal();
    retval = krb5_mk_error(context, &errpkt, scratch);
    free(errpkt.text.data);
    krb5_free_data(context, e_data_asn1);
    krb5_free_data(context, fast_edata);
    if (retval)
        free(scratch);
    else
        *response = scratch;

    return retval;
}

/* KDC options that require a second ticket */
#define STKT_OPTIONS (KDC_OPT_CNAME_IN_ADDL_TKT | KDC_OPT_ENC_TKT_IN_SKEY)
/*
 * If req is a second-ticket request and a second ticket is present, decrypt
 * it.  Set *stkt_out to an alias to the ticket with populated enc_part2.  Set
 * *server_out to the server DB entry and *key_out to the ticket decryption
 * key.
 */
static krb5_error_code
decrypt_2ndtkt(krb5_context context, krb5_kdc_req *req, krb5_flags flags,
               krb5_db_entry *local_tgt, krb5_keyblock *local_tgt_key,
               const krb5_ticket **stkt_out, krb5_pac *pac_out,
               krb5_db_entry **server_out, krb5_keyblock **key_out,
               const char **status)
{
    krb5_error_code retval;
    krb5_db_entry *server = NULL;
    krb5_keyblock *key = NULL;
    krb5_kvno kvno;
    krb5_ticket *stkt;

    *stkt_out = NULL;
    *pac_out = NULL;
    *server_out = NULL;
    *key_out = NULL;

    if (!(req->kdc_options & STKT_OPTIONS) || req->second_ticket == NULL ||
        req->second_ticket[0] == NULL)
        return 0;

    stkt = req->second_ticket[0];
    retval = kdc_get_server_key(context, stkt, flags, TRUE, &server, &key,
                                &kvno);
    if (retval != 0) {
        *status = "2ND_TKT_SERVER";
        goto cleanup;
    }
    retval = krb5_decrypt_tkt_part(context, key, stkt);
    if (retval != 0) {
        *status = "2ND_TKT_DECRYPT";
        goto cleanup;
    }
    retval = get_verified_pac(context, stkt->enc_part2, server, key, local_tgt,
                              local_tgt_key, pac_out);
    if (retval != 0) {
        *status = "2ND_TKT_PAC";
        goto cleanup;
    }
    *stkt_out = stkt;
    *server_out = server;
    *key_out = key;
    server = NULL;
    key = NULL;

cleanup:
    krb5_db_free_principal(context, server);
    krb5_free_keyblock(context, key);
    return retval;
}

static krb5_error_code
get_2ndtkt_enctype(krb5_kdc_req *req, krb5_enctype *useenctype,
                   const char **status)
{
    krb5_enctype etype;
    krb5_ticket *stkt = req->second_ticket[0];
    int i;

    etype = stkt->enc_part2->session->enctype;
    if (!krb5_c_valid_enctype(etype)) {
        *status = "BAD_ETYPE_IN_2ND_TKT";
        return KRB5KDC_ERR_ETYPE_NOSUPP;
    }
    for (i = 0; i < req->nktypes; i++) {
        if (req->ktype[i] == etype) {
            *useenctype = etype;
            break;
        }
    }
    return 0;
}

static krb5_error_code
gen_session_key(krb5_context context, krb5_kdc_req *req, krb5_db_entry *server,
                krb5_keyblock *skey, const char **status)
{
    krb5_error_code retval;
    krb5_enctype useenctype = 0;

    /*
     * Some special care needs to be taken in the user-to-user
     * case, since we don't know what keytypes the application server
     * which is doing user-to-user authentication can support.  We
     * know that it at least must be able to support the encryption
     * type of the session key in the TGT, since otherwise it won't be
     * able to decrypt the U2U ticket!  So we use that in preference
     * to anything else.
     */
    if (req->kdc_options & KDC_OPT_ENC_TKT_IN_SKEY) {
        retval = get_2ndtkt_enctype(req, &useenctype, status);
        if (retval != 0)
            return retval;
    }
    if (useenctype == 0) {
        useenctype = select_session_keytype(context, server,
                                            req->nktypes, req->ktype);
    }
    if (useenctype == 0) {
        /* unsupported ktype */
        *status = "BAD_ENCRYPTION_TYPE";
        return KRB5KDC_ERR_ETYPE_NOSUPP;
    }

    return krb5_c_make_random_key(context, useenctype, skey);
}

/*
 * The request seems to be for a ticket-granting service somewhere else,
 * but we don't have a ticket for the final TGS.  Try to give the requestor
 * some intermediate realm.
 */
static krb5_error_code
find_alternate_tgs(krb5_context context, krb5_principal princ,
                   krb5_db_entry **server_ptr, const char **status)
{
    krb5_error_code retval;
    krb5_principal *plist = NULL, *pl2;
    krb5_data tmp;
    krb5_db_entry *server = NULL;

    *server_ptr = NULL;
    assert(is_cross_tgs_principal(princ));
    retval = krb5_walk_realm_tree(context, &princ->realm, &princ->data[1],
                                  &plist, KRB5_REALM_BRANCH_CHAR);
    if (retval)
        goto cleanup;
    /* move to the end */
    for (pl2 = plist; *pl2; pl2++);

    /* the first entry in this array is for krbtgt/local@local, so we
       ignore it */
    while (--pl2 > plist) {
        tmp = *krb5_princ_realm(context, *pl2);
        krb5_princ_set_realm(context, *pl2, &princ->realm);
        retval = db_get_svc_princ(context, *pl2, 0, &server, status);
        krb5_princ_set_realm(context, *pl2, &tmp);
        if (retval == KRB5_KDB_NOENTRY)
            continue;
        else if (retval)
            goto cleanup;

        log_tgs_alt_tgt(context, server->princ);
        *server_ptr = server;
        server = NULL;
        goto cleanup;
    }
cleanup:
    if (retval == 0 && *server_ptr == NULL)
        retval = KRB5_KDB_NOENTRY;
    if (retval != 0)
        *status = "UNKNOWN_SERVER";

    krb5_free_realm_tree(context, plist);
    krb5_db_free_principal(context, server);
    return retval;
}

/* Return true if item is an element of the space/comma-separated list. */
static krb5_boolean
in_list(const char *list, const char *item)
{
    const char *p;
    int len = strlen(item);

    if (list == NULL)
        return FALSE;
    for (p = strstr(list, item); p != NULL; p = strstr(p + 1, item)) {
        if ((p == list || isspace((unsigned char)p[-1]) || p[-1] == ',') &&
            (p[len] == '\0' || isspace((unsigned char)p[len]) ||
             p[len] == ','))
                return TRUE;
    }
    return FALSE;
}

/*
 * Check whether the request satisfies the conditions for generating a referral
 * TGT.  The caller checks whether the hostname component looks like a FQDN.
 */
static krb5_boolean
is_referral_req(kdc_realm_t *realm, krb5_kdc_req *request)
{
    krb5_boolean ret = FALSE;
    char *stype = NULL;
    char *hostbased = realm->realm_hostbased;
    char *no_referral = realm->realm_no_referral;

    if (!(request->kdc_options & KDC_OPT_CANONICALIZE))
        return FALSE;

    if (request->kdc_options & KDC_OPT_ENC_TKT_IN_SKEY)
        return FALSE;

    if (request->server->length != 2)
        return FALSE;

    stype = data2string(&request->server->data[0]);
    if (stype == NULL)
        return FALSE;
    switch (request->server->type) {
    case KRB5_NT_UNKNOWN:
        /* Allow referrals for NT-UNKNOWN principals, if configured. */
        if (!in_list(hostbased, stype) && !in_list(hostbased, "*"))
            goto cleanup;
        /* FALLTHROUGH */
    case KRB5_NT_SRV_HST:
    case KRB5_NT_SRV_INST:
        /* Deny referrals for specific service types, if configured. */
        if (in_list(no_referral, stype) || in_list(no_referral, "*"))
            goto cleanup;
        ret = TRUE;
        break;
    default:
        goto cleanup;
    }
cleanup:
    free(stype);
    return ret;
}

/*
 * Find a remote realm TGS principal for an unknown host-based service
 * principal.
 */
static krb5_int32
find_referral_tgs(kdc_realm_t *realm, krb5_kdc_req *request,
                  krb5_principal *krbtgt_princ)
{
    krb5_context context = realm->realm_context;
    krb5_error_code retval = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
    char **realms = NULL, *hostname = NULL;
    krb5_data srealm = request->server->realm;

    if (!is_referral_req(realm, request))
        goto cleanup;

    hostname = data2string(&request->server->data[1]);
    if (hostname == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }
    /* If the hostname doesn't contain a '.', it's not a FQDN. */
    if (strchr(hostname, '.') == NULL)
        goto cleanup;
    retval = krb5_get_host_realm(context, hostname, &realms);
    if (retval) {
        /* no match found */
        kdc_err(context, retval, "unable to find realm of host");
        goto cleanup;
    }
    /* Don't return a referral to the empty realm or the service realm. */
    if (realms == NULL || realms[0] == NULL || *realms[0] == '\0' ||
        data_eq_string(srealm, realms[0])) {
        retval = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
        goto cleanup;
    }
    retval = krb5_build_principal(context, krbtgt_princ,
                                  srealm.length, srealm.data,
                                  "krbtgt", realms[0], (char *)0);
cleanup:
    krb5_free_host_realm(context, realms);
    free(hostname);

    return retval;
}

static krb5_error_code
db_get_svc_princ(krb5_context ctx, krb5_principal princ,
                 krb5_flags flags, krb5_db_entry **server,
                 const char **status)
{
    krb5_error_code ret;

    ret = krb5_db_get_principal(ctx, princ, flags, server);
    if (ret == KRB5_KDB_CANTLOCK_DB)
        ret = KRB5KDC_ERR_SVC_UNAVAILABLE;
    if (ret != 0) {
        *status = "LOOKING_UP_SERVER";
    }
    return ret;
}

static krb5_error_code
search_sprinc(kdc_realm_t *realm, krb5_kdc_req *req,
              krb5_flags flags, krb5_db_entry **server, const char **status)
{
    krb5_context context = realm->realm_context;
    krb5_error_code ret;
    krb5_principal princ = req->server;
    krb5_principal reftgs = NULL;
    krb5_boolean allow_referral;

    /* Do not allow referrals for u2u or ticket modification requests, because
     * the server is supposed to match an already-issued ticket. */
    allow_referral = !(req->kdc_options & NO_REFERRAL_OPTION);
    if (!allow_referral)
        flags &= ~KRB5_KDB_FLAG_REFERRAL_OK;

    ret = db_get_svc_princ(context, princ, flags, server, status);
    if (ret == 0 || ret != KRB5_KDB_NOENTRY || !allow_referral)
        goto cleanup;

    if (!is_cross_tgs_principal(req->server)) {
        ret = find_referral_tgs(realm, req, &reftgs);
        if (ret != 0)
            goto cleanup;
        ret = db_get_svc_princ(context, reftgs, flags, server, status);
        if (ret == 0 || ret != KRB5_KDB_NOENTRY)
            goto cleanup;

        princ = reftgs;
    }
    ret = find_alternate_tgs(context, princ, server, status);

cleanup:
    if (ret != 0 && ret != KRB5KDC_ERR_SVC_UNAVAILABLE) {
        ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
        if (*status == NULL)
            *status = "LOOKING_UP_SERVER";
    }
    krb5_free_principal(context, reftgs);
    return ret;
}

/*
 * Transfer ownership of *reqptr to *t and fill *t with information about the
 * request.  Decode the PA-TGS-REQ header ticket and the second ticket if
 * applicable, and decode and verify their PACs if present.  Decode and verify
 * the S4U2Self request pa-data if present.  Extract authentication indicators
 * from the subject ticket.  Construct the transited list implied by the
 * request.
 */
static krb5_error_code
gather_tgs_req_info(kdc_realm_t *realm, krb5_kdc_req **reqptr, krb5_data *pkt,
                    const krb5_fulladdr *from,
                    struct kdc_request_state *fast_state,
                    krb5_audit_state *au_state, struct tgs_req_info *t,
                    const char **status)
{
    krb5_context context = realm->realm_context;
    krb5_error_code ret;
    krb5_pa_data *pa_tgs_req;
    unsigned int s_flags;
    krb5_enc_tkt_part *header_enc;
    krb5_data d;

    /* Transfer ownership of *reqptr to *t. */
    t->req = *reqptr;
    *reqptr = NULL;

    if (t->req->msg_type != KRB5_TGS_REQ)
        return KRB5_BADMSGTYPE;

    /* Initially set t->sprinc to the outer request server, for logging of
     * early failures. */
    t->sprinc = t->req->server;

    /* Read the PA-TGS-REQ authenticator and decrypt the header ticket. */
    ret = kdc_process_tgs_req(realm, t->req, from, pkt, &t->header_tkt,
                              &t->header_server, &t->header_key, &t->subkey,
                              &pa_tgs_req);
    if (t->header_tkt != NULL && t->header_tkt->enc_part2 != NULL)
        t->cprinc = t->header_tkt->enc_part2->client;
    if (ret) {
        *status = "PROCESS_TGS";
        return ret;
    }
    ret = kau_make_tkt_id(context, t->header_tkt, &au_state->tkt_in_id);
    if (ret)
        return ret;
    header_enc = t->header_tkt->enc_part2;

    /* If PA-FX-FAST-REQUEST padata is present, replace t->req with the inner
     * request body. */
    d = make_data(pa_tgs_req->contents, pa_tgs_req->length);
    ret = kdc_find_fast(&t->req, &d, t->subkey, header_enc->session,
                        fast_state, NULL);
    if (ret) {
        *status = "FIND_FAST";
        return ret;
    }
    /* Reset t->sprinc for the inner body and check it. */
    t->sprinc = t->req->server;
    if (t->sprinc == NULL) {
        *status = "NULL_SERVER";
        return KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
    }

    /* The header ticket server is usually a TGT, but if it is not, fetch the
     * local TGT for the realm.  Get the decrypted first local TGT key. */
    ret = get_local_tgt(context, &t->sprinc->realm, t->header_server,
                        &t->local_tgt, &t->local_tgt_storage,
                        &t->local_tgt_key);
    if (ret) {
        *status = "GET_LOCAL_TGT";
        return ret;
    }

    /* Decode and verify the header ticket PAC. */
    ret = get_verified_pac(context, header_enc, t->header_server,
                           t->header_key, t->local_tgt, &t->local_tgt_key,
                           &t->header_pac);
    if (ret) {
        *status = "HEADER_PAC";
        return ret;
    }

    au_state->request = t->req;
    au_state->stage = SRVC_PRINC;

    /* Look up the server principal entry, or a referral/alternate TGT.  Reset
     * t->sprinc to the canonical server name (its final value). */
    s_flags = (t->req->kdc_options & KDC_OPT_CANONICALIZE) ?
        KRB5_KDB_FLAG_REFERRAL_OK : 0;
    ret = search_sprinc(realm, t->req, s_flags, &t->server, status);
    if (ret)
        return ret;
    t->sprinc = t->server->princ;

    /* If we got a cross-realm TGS which is not the requested server, we are
     * issuing a referral (or alternate TGT, which we treat similarly). */
    if (is_cross_tgs_principal(t->server->princ) &&
        !krb5_principal_compare(context, t->req->server, t->server->princ))
        t->flags |= KRB5_KDB_FLAG_ISSUING_REFERRAL;

    /* Mark the request as cross-realm if the header ticket server is not from
     * this realm. */
    if (!data_eq(t->header_server->princ->realm, t->sprinc->realm))
        t->flags |= KRB5_KDB_FLAG_CROSS_REALM;

    t->is_referral = (t->flags & KRB5_KDB_FLAG_ISSUING_REFERRAL);
    t->is_crossrealm = (t->flags & KRB5_KDB_FLAG_CROSS_REALM);

    /* If S4U2Self padata is present, read it to get the requested principal
     * name.  Look up the requested client if it is in this realm. */
    ret = kdc_process_s4u2self_req(context, t->req, t->server, t->subkey,
                                   header_enc->session, &t->s4u2self,
                                   &t->client, status);
    if (t->s4u2self != NULL || ret) {
        if (t->s4u2self != NULL)
            au_state->s4u2self_user = t->s4u2self->user_id.user;
        au_state->status = *status;
        kau_s4u2self(context, !ret, au_state);
        au_state->s4u2self_user = NULL;
    }
    if (ret)
        return ret;
    if (t->s4u2self != NULL) {
        t->flags |= KRB5_KDB_FLAG_PROTOCOL_TRANSITION;
        t->s4u_cprinc = t->s4u2self->user_id.user;

        /*
         * For consistency with Active Directory, don't allow authorization
         * data to be disabled if S4U2Self is requested.  The requesting
         * service likely needs a PAC for an S4U2Proxy operation, even if it
         * doesn't need authorization data in tickets received from clients.
         */
        t->server->attributes &= ~KRB5_KDB_NO_AUTH_DATA_REQUIRED;
    }

    /* For U2U or S4U2Proxy requests, decrypt the second ticket and read its
     * PAC. */
    ret = decrypt_2ndtkt(context, t->req, t->flags, t->local_tgt,
                         &t->local_tgt_key, &t->stkt, &t->stkt_pac,
                         &t->stkt_server, &t->stkt_server_key, status);
    if (ret)
        return ret;

    /* Determine the subject ticket and set the authtime for logging.  For
     * S4U2Proxy requests determine the requested client principal. */
    if (t->req->kdc_options & KDC_OPT_CNAME_IN_ADDL_TKT) {
        t->flags |= KRB5_KDB_FLAG_CONSTRAINED_DELEGATION;
        ret = kau_make_tkt_id(context, t->stkt, &au_state->evid_tkt_id);
        if (ret)
            return ret;
        if (t->is_crossrealm) {
            /* For cross-realm S4U2PROXY requests, the second ticket is a
             * cross TGT with the requested client principal in its PAC. */
            if (t->stkt_pac == NULL ||
                get_pac_princ_with_realm(context, t->stkt_pac,
                                         &t->stkt_pac_client, NULL) != 0) {
                au_state->status = *status = "RBCD_PAC_PRINC";
                au_state->violation = PROT_CONSTRAINT;
                kau_s4u2proxy(context, FALSE, au_state);
                return KRB5KDC_ERR_BADOPTION;
            }
            t->s4u_cprinc = t->stkt_pac_client;
        } else {
            /* Otherwise the requested client is the evidence ticket client. */
            t->s4u_cprinc = t->stkt->enc_part2->client;
        }
        t->subject_tkt = t->stkt->enc_part2;
    } else {
        t->subject_tkt = header_enc;
    }
    t->authtime = t->subject_tkt->times.authtime;

    /* For final S4U requests (either type) the issued ticket will be for the
     * requested name; otherwise it will be for the header ticket client. */
    t->tkt_client = ((t->flags & KRB5_KDB_FLAGS_S4U) && !t->is_referral) ?
        t->s4u_cprinc : header_enc->client;

    if (t->s4u2self == NULL) {
        /* Extract auth indicators from the subject ticket.  Skip this for
         * S4U2Self requests as the subject didn't authenticate. */
        ret = get_auth_indicators(context, t->subject_tkt, t->local_tgt,
                                  &t->local_tgt_key, &t->auth_indicators);
        if (ret) {
            *status = "GET_AUTH_INDICATORS";
            return ret;
        }

        if (!(t->server->attributes & KRB5_KDB_NO_AUTH_DATA_REQUIRED)) {
            /* Try to look up the subject principal so that KDB modules can add
             * additional authdata.  Ask the KDB to map foreign principals. */
            assert(t->client == NULL);
            (void)krb5_db_get_principal(context, t->subject_tkt->client,
                                        t->flags | KRB5_KDB_FLAG_CLIENT |
                                        KRB5_KDB_FLAG_MAP_PRINCIPALS,
                                        &t->client);
        }
    }

    /*
     * Compute the transited list implied by the request.  Use the existing
     * transited list if the realm of the header ticket server is the same as
     * the subject or server realm.
     */
    if (!t->is_crossrealm ||
        data_eq(t->header_tkt->server->realm, t->tkt_client->realm)) {
        t->transited = header_enc->transited;
    } else {
        if (header_enc->transited.tr_type != KRB5_DOMAIN_X500_COMPRESS) {
            *status = "VALIDATE_TRANSIT_TYPE";
            return KRB5KDC_ERR_TRTYPE_NOSUPP;
        }
        ret = add_to_transited(&header_enc->transited.tr_contents,
                               &t->new_transited, t->header_tkt->server,
                               t->tkt_client, t->req->server);
        if (ret) {
            *status = "ADD_TO_TRANSITED_LIST";
            return ret;
        }
        t->transited.tr_type = KRB5_DOMAIN_X500_COMPRESS;
        t->transited.tr_contents = t->new_transited;
    }

    return 0;
}

/* Fill in *times_out with the times of the ticket to be issued.  Set the
 * TKT_FLG_RENEWABLE bit in *tktflags if the ticket will be renewable. */
static void
compute_ticket_times(kdc_realm_t *realm, struct tgs_req_info *t,
                     krb5_timestamp kdc_time, krb5_flags *tktflags,
                     krb5_ticket_times *times)
{
    krb5_timestamp hstarttime;
    krb5_deltat hlife;
    krb5_ticket_times *htimes = &t->header_tkt->enc_part2->times;

    if (t->req->kdc_options & KDC_OPT_VALIDATE) {
        /* Validation requests preserve the header ticket times. */
        *times = *htimes;
        return;
    }

    /* Preserve the authtime from the subject ticket. */
    times->authtime = t->authtime;

    times->starttime = (t->req->kdc_options & KDC_OPT_POSTDATED) ?
        t->req->from : kdc_time;

    if (t->req->kdc_options & KDC_OPT_RENEW) {
        /* Give the new ticket the same lifetime as the header ticket, but no
         * later than the renewable end time. */
        hstarttime = htimes->starttime ? htimes->starttime : htimes->authtime;
        hlife = ts_delta(htimes->endtime, hstarttime);
        times->endtime = ts_min(htimes->renew_till,
                                ts_incr(times->starttime, hlife));
    } else {
        kdc_get_ticket_endtime(realm, times->starttime, htimes->endtime,
                               t->req->till, t->client, t->server,
                               &times->endtime);
    }

    kdc_get_ticket_renewtime(realm, t->req, t->header_tkt->enc_part2,
                             t->client, t->server, tktflags, times);

    /* starttime is optional, and treated as authtime if not present.
     * so we can omit it if it matches. */
    if (times->starttime == times->authtime)
        times->starttime = 0;
}

/* Check the request in *t against semantic protocol constraints and local
 * policy.  Determine flags and times for the ticket to be issued. */
static krb5_error_code
check_tgs_req(kdc_realm_t *realm, struct tgs_req_info *t,
              krb5_audit_state *au_state, krb5_flags *tktflags,
              krb5_ticket_times *times, const char **status,
              krb5_pa_data ***e_data)
{
    krb5_context context = realm->realm_context;
    krb5_error_code ret;
    krb5_timestamp kdc_time;

    au_state->stage = VALIDATE_POL;

    ret = krb5_timeofday(context, &kdc_time);
    if (ret)
        return ret;

    ret = check_tgs_constraints(realm, t->req, t->server, t->header_tkt,
                                t->header_pac, t->stkt, t->stkt_pac,
                                t->stkt_server, kdc_time, t->s4u2self,
                                t->client, t->is_crossrealm, t->is_referral,
                                status, e_data);
    if (ret) {
        au_state->violation = PROT_CONSTRAINT;
        return ret;
    }

    ret = check_tgs_policy(realm, t->req, t->server, t->header_tkt,
                           t->header_pac, t->stkt, t->stkt_pac,
                           t->stkt_pac_client, t->stkt_server, kdc_time,
                           t->is_crossrealm, t->is_referral, status, e_data);
    if (ret) {
        au_state->violation = LOCAL_POLICY;
        if (t->flags & KRB5_KDB_FLAG_CONSTRAINED_DELEGATION) {
            au_state->status = *status;
            kau_s4u2proxy(context, FALSE, au_state);
        }
        return ret;
    }

    /* Check auth indicators from the subject ticket, except for S4U2Self
     * requests (where the client didn't authenticate). */
    if (t->s4u2self == NULL) {
        ret = check_indicators(context, t->server, t->auth_indicators);
        if (ret) {
            *status = "HIGHER_AUTHENTICATION_REQUIRED";
            return ret;
        }
    }

    *tktflags = get_ticket_flags(t->req->kdc_options, t->client, t->server,
                                 t->header_tkt->enc_part2);
    compute_ticket_times(realm, t, kdc_time, tktflags, times);

    /* For S4U2Self requests, check if we need to suppress the forwardable
     * ticket flag. */
    if (t->s4u2self != NULL && !t->is_referral) {
        ret = s4u2self_forwardable(context, t->server, tktflags);
        if (ret)
            return ret;
    }

    /* Consult kdcpolicy modules, giving them a chance to modify the times of
     * the issued ticket. */
    ret = check_kdcpolicy_tgs(context, t->req, t->server, t->header_tkt,
                              t->auth_indicators, kdc_time, times, status);
    if (ret)
        return ret;

    if (!(t->req->kdc_options & KDC_OPT_DISABLE_TRANSITED_CHECK)) {
        /* Check the transited path for the issued ticket and set the
         * transited-policy-checked flag if successful. */
        ret = kdc_check_transited_list(context, &t->transited.tr_contents,
                                       &t->subject_tkt->client->realm,
                                       &t->req->server->realm);
        if (ret) {
            /* Log the transited-check failure and continue. */
            log_tgs_badtrans(context, t->cprinc, t->sprinc,
                             &t->transited.tr_contents, ret);
        } else {
            *tktflags |= TKT_FLG_TRANSIT_POLICY_CHECKED;
        }
    } else {
        krb5_klog_syslog(LOG_INFO, _("not checking transit path"));
    }

    /* By default, reject the request if the transited path was not checked
     * successfully. */
    if (realm->realm_reject_bad_transit &&
        !(*tktflags & TKT_FLG_TRANSIT_POLICY_CHECKED)) {
        *status = "BAD_TRANSIT";
        au_state->violation = LOCAL_POLICY;
        return KRB5KDC_ERR_POLICY;
    }

    return 0;
}

/* Construct a response issuing a ticket for the request in *t, using tktflags
 * and *times for the ticket flags and times. */
static krb5_error_code
tgs_issue_ticket(kdc_realm_t *realm, struct tgs_req_info *t,
                 krb5_flags tktflags, krb5_ticket_times *times, krb5_data *pkt,
                 const krb5_fulladdr *from,
                 struct kdc_request_state *fast_state,
                 krb5_audit_state *au_state, const char **status,
                 krb5_data **response)
{
    krb5_context context = realm->realm_context;
    krb5_error_code ret;
    krb5_keyblock session_key = { 0 }, server_key = { 0 };
    krb5_keyblock *ticket_encrypting_key, *subject_key;
    krb5_keyblock *initial_reply_key, *fast_reply_key = NULL;
    krb5_enc_tkt_part enc_tkt_reply = { 0 };
    krb5_ticket ticket_reply = { 0 };
    krb5_enc_kdc_rep_part reply_encpart = { 0 };
    krb5_kdc_rep reply = { 0 };
    krb5_pac subject_pac;
    krb5_db_entry *subject_server;
    krb5_enc_tkt_part *header_enc_tkt = t->header_tkt->enc_part2;
    krb5_last_req_entry nolrentry = { KV5M_LAST_REQ_ENTRY, KRB5_LRQ_NONE, 0 };
    krb5_last_req_entry *nolrarray[2] = { &nolrentry, NULL };

    au_state->stage = ISSUE_TKT;

    ret = gen_session_key(context, t->req, t->server, &session_key, status);
    if (ret)
        goto cleanup;

    if (t->flags & KRB5_KDB_FLAG_CONSTRAINED_DELEGATION) {
        subject_pac = t->stkt_pac;
        subject_server = t->stkt_server;
        subject_key = t->stkt_server_key;
    } else {
        subject_pac = t->header_pac;
        subject_server = t->header_server;
        subject_key = t->header_key;
    }

    initial_reply_key = (t->subkey != NULL) ? t->subkey :
        t->header_tkt->enc_part2->session;

    if (t->req->kdc_options & KDC_OPT_ENC_TKT_IN_SKEY) {
        /* For user-to-user, encrypt the ticket with the second ticket's
         * session key. */
        ticket_encrypting_key = t->stkt->enc_part2->session;
    } else {
        /* Otherwise encrypt the ticket with the server entry's first long-term
         * key. */
        ret = get_first_current_key(context, t->server, &server_key);
        if (ret) {
            *status = "FINDING_SERVER_KEY";
            goto cleanup;
        }
        ticket_encrypting_key = &server_key;
    }

    if (t->req->kdc_options & (KDC_OPT_VALIDATE | KDC_OPT_RENEW)) {
        /* Copy the header ticket server and all enc-part fields except for
         * authorization data. */
        ticket_reply.server = t->header_tkt->server;
        enc_tkt_reply = *t->header_tkt->enc_part2;
        enc_tkt_reply.authorization_data = NULL;
    } else {
        if (t->req->kdc_options & (KDC_OPT_FORWARDED | KDC_OPT_PROXY)) {
            /* Include the requested addresses in the ticket and reply. */
            enc_tkt_reply.caddrs = t->req->addresses;
            reply_encpart.caddrs = t->req->addresses;
        } else {
            /* Use the header ticket addresses and omit them from the reply. */
            enc_tkt_reply.caddrs = header_enc_tkt->caddrs;
            reply_encpart.caddrs = NULL;
        }

        ticket_reply.server = t->is_referral ? t->sprinc : t->req->server;
    }

    enc_tkt_reply.flags = tktflags;
    enc_tkt_reply.times = *times;
    enc_tkt_reply.client = t->tkt_client;
    enc_tkt_reply.session = &session_key;
    enc_tkt_reply.transited = t->transited;

    ret = handle_authdata(realm, t->flags, t->client, t->server,
                          subject_server, t->local_tgt, &t->local_tgt_key,
                          initial_reply_key, ticket_encrypting_key,
                          subject_key, NULL, pkt, t->req, t->s4u_cprinc,
                          subject_pac, t->subject_tkt, &t->auth_indicators,
                          &enc_tkt_reply);
    if (ret) {
        krb5_klog_syslog(LOG_INFO, _("TGS_REQ : handle_authdata (%d)"), ret);
        *status = "HANDLE_AUTHDATA";
        goto cleanup;
    }

    ticket_reply.enc_part2 = &enc_tkt_reply;

    ret = krb5_encrypt_tkt_part(context, ticket_encrypting_key, &ticket_reply);
    if (ret)
        goto cleanup;

    if (t->req->kdc_options & KDC_OPT_ENC_TKT_IN_SKEY) {
        ticket_reply.enc_part.kvno = 0;
        kau_u2u(context, TRUE, au_state);
    } else {
        ticket_reply.enc_part.kvno = current_kvno(t->server);
    }

    au_state->stage = ENCR_REP;

    if (t->s4u2self != NULL &&
        krb5int_find_pa_data(context, t->req->padata,
                             KRB5_PADATA_S4U_X509_USER) != NULL) {
        /* Add an S4U2Self response to the encrypted padata (skipped if the
         * request only included PA-FOR-USER padata). */
        ret = kdc_make_s4u2self_rep(context, t->subkey,
                                    t->header_tkt->enc_part2->session,
                                    t->s4u2self, &reply, &reply_encpart);
        if (ret)
            goto cleanup;
    }

    reply_encpart.session = &session_key;
    reply_encpart.nonce = t->req->nonce;
    reply_encpart.times = enc_tkt_reply.times;
    reply_encpart.last_req = nolrarray;
    reply_encpart.key_exp = 0;
    reply_encpart.flags = enc_tkt_reply.flags;
    reply_encpart.server = ticket_reply.server;

    reply.msg_type = KRB5_TGS_REP;
    reply.client = enc_tkt_reply.client;
    reply.ticket = &ticket_reply;
    reply.enc_part.kvno = 0;
    reply.enc_part.enctype = initial_reply_key->enctype;
    ret = kdc_fast_response_handle_padata(fast_state, t->req, &reply,
                                          initial_reply_key->enctype);
    if (ret)
        goto cleanup;
    ret = kdc_fast_handle_reply_key(fast_state, initial_reply_key,
                                    &fast_reply_key);
    if (ret)
        goto cleanup;
    ret = return_enc_padata(context, pkt, t->req, fast_reply_key, t->server,
                            &reply_encpart,
                            t->is_referral &&
                            (t->req->kdc_options & KDC_OPT_CANONICALIZE));
    if (ret) {
        *status = "KDC_RETURN_ENC_PADATA";
        goto cleanup;
    }

    ret = kau_make_tkt_id(context, &ticket_reply, &au_state->tkt_out_id);
    if (ret)
        goto cleanup;

    if (kdc_fast_hide_client(fast_state))
        reply.client = (krb5_principal)krb5_anonymous_principal();
    ret = krb5_encode_kdc_rep(context, KRB5_TGS_REP, &reply_encpart,
                              t->subkey != NULL, fast_reply_key, &reply,
                              response);
    if (ret)
        goto cleanup;

    log_tgs_req(context, from, t->req, &reply, t->cprinc, t->sprinc,
                t->s4u_cprinc, t->authtime, t->flags, "ISSUE", 0, NULL);
    au_state->status = "ISSUE";
    au_state->reply = &reply;
    if (t->flags & KRB5_KDB_FLAG_CONSTRAINED_DELEGATION)
        kau_s4u2proxy(context, TRUE, au_state);
    kau_tgs_req(context, TRUE, au_state);
    au_state->reply = NULL;

cleanup:
    zapfree(ticket_reply.enc_part.ciphertext.data,
            ticket_reply.enc_part.ciphertext.length);
    zapfree(reply.enc_part.ciphertext.data, reply.enc_part.ciphertext.length);
    krb5_free_pa_data(context, reply.padata);
    krb5_free_pa_data(context, reply_encpart.enc_padata);
    krb5_free_authdata(context, enc_tkt_reply.authorization_data);
    krb5_free_keyblock_contents(context, &session_key);
    krb5_free_keyblock_contents(context, &server_key);
    krb5_free_keyblock(context, fast_reply_key);
    return ret;
}

static void
free_req_info(krb5_context context, struct tgs_req_info *t)
{
    krb5_free_kdc_req(context, t->req);
    krb5_free_ticket(context, t->header_tkt);
    krb5_db_free_principal(context, t->header_server);
    krb5_free_keyblock(context, t->header_key);
    krb5_free_keyblock(context, t->subkey);
    krb5_pac_free(context, t->header_pac);
    krb5_pac_free(context, t->stkt_pac);
    krb5_db_free_principal(context, t->stkt_server);
    krb5_free_keyblock(context, t->stkt_server_key);
    krb5_db_free_principal(context, t->local_tgt_storage);
    krb5_free_keyblock_contents(context, &t->local_tgt_key);
    krb5_db_free_principal(context, t->server);
    krb5_db_free_principal(context, t->client);
    krb5_free_pa_s4u_x509_user(context, t->s4u2self);
    krb5_free_principal(context, t->stkt_pac_client);
    k5_free_data_ptr_list(t->auth_indicators);
    krb5_free_data_contents(context, &t->new_transited);
}

krb5_error_code
process_tgs_req(krb5_kdc_req *request, krb5_data *pkt,
                const krb5_fulladdr *from, kdc_realm_t *realm,
                krb5_data **response)
{
    krb5_context context = realm->realm_context;
    krb5_error_code ret;
    struct tgs_req_info t = { 0 };
    struct kdc_request_state *fast_state = NULL;
    krb5_audit_state *au_state = NULL;
    krb5_pa_data **e_data = NULL;
    krb5_flags tktflags;
    krb5_ticket_times times = { 0 };
    const char *emsg = NULL, *status = NULL;

    ret = kdc_make_rstate(realm, &fast_state);
    if (ret)
        goto cleanup;
    ret = kau_init_kdc_req(context, request, from, &au_state);
    if (ret)
        goto cleanup;
    kau_tgs_req(context, TRUE, au_state);

    ret = gather_tgs_req_info(realm, &request, pkt, from, fast_state, au_state,
                              &t, &status);
    if (ret)
        goto cleanup;

    ret = check_tgs_req(realm, &t, au_state, &tktflags, &times, &status,
                        &e_data);
    if (ret)
        goto cleanup;

    ret = tgs_issue_ticket(realm, &t, tktflags, &times, pkt, from, fast_state,
                           au_state, &status, response);
    if (ret)
        goto cleanup;

cleanup:
    if (status == NULL)
        status = "UNKNOWN_REASON";

    if (ret) {
        emsg = krb5_get_error_message(context, ret);
        log_tgs_req(context, from, t.req, NULL, t.cprinc, t.sprinc,
                    t.s4u_cprinc, t.authtime, t.flags, status, ret, emsg);
        krb5_free_error_message(context, emsg);

        if (au_state != NULL) {
            au_state->status = status;
            kau_tgs_req(context, FALSE, au_state);
        }
    }

    if (ret && fast_state != NULL) {
        ret = prepare_error_tgs(fast_state, t.req, t.header_tkt, ret,
                                (t.server != NULL) ? t.server->princ : NULL,
                                response, status, e_data);
    }

    krb5_free_kdc_req(context, request);
    kdc_free_rstate(fast_state);
    kau_free_kdc_req(au_state);
    free_req_info(context, &t);
    krb5_free_pa_data(context, e_data);
    return ret;
}
