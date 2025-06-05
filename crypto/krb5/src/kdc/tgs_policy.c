/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/tgs_policy.c */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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
#include "kdc_util.h"

/*
 * Routines that validate a TGS request; checks a lot of things.  :-)
 *
 * Returns a Kerberos protocol error number, which is _not_ the same
 * as a com_err error number!
 */

struct tgsflagrule {
    krb5_flags reqflags;        /* Flag(s) in TGS-REQ */
    krb5_flags checkflag;       /* Flags to check against */
    char *status;               /* Status string */
    int err;                    /* Protocol error code */
};

/* Service principal TGS policy checking functions */
typedef int (check_tgs_svc_pol_fn)(krb5_kdc_req *, krb5_db_entry *,
                                   krb5_ticket *, krb5_timestamp,
                                   const char **);

static check_tgs_svc_pol_fn check_tgs_svc_deny_opts;
static check_tgs_svc_pol_fn check_tgs_svc_deny_all;
static check_tgs_svc_pol_fn check_tgs_svc_reqd_flags;
static check_tgs_svc_pol_fn check_tgs_svc_time;

static check_tgs_svc_pol_fn * const svc_pol_fns[] = {
    check_tgs_svc_deny_opts, check_tgs_svc_deny_all, check_tgs_svc_reqd_flags,
    check_tgs_svc_time
};

static const struct tgsflagrule tgsflagrules[] = {
    { KDC_OPT_FORWARDED, TKT_FLG_FORWARDABLE,
      "TGT NOT FORWARDABLE", KRB5KDC_ERR_BADOPTION },
    { KDC_OPT_PROXY, TKT_FLG_PROXIABLE,
      "TGT NOT PROXIABLE", KRB5KDC_ERR_BADOPTION },
    { (KDC_OPT_ALLOW_POSTDATE | KDC_OPT_POSTDATED), TKT_FLG_MAY_POSTDATE,
      "TGT NOT POSTDATABLE", KRB5KDC_ERR_BADOPTION },
    { KDC_OPT_VALIDATE, TKT_FLG_INVALID,
      "VALIDATE VALID TICKET", KRB5KDC_ERR_BADOPTION },
    { KDC_OPT_RENEW, TKT_FLG_RENEWABLE,
      "TICKET NOT RENEWABLE", KRB5KDC_ERR_BADOPTION }
};

/*
 * Some TGS-REQ options require that the ticket have corresponding flags set.
 */
static krb5_error_code
check_tgs_opts(krb5_kdc_req *req, krb5_ticket *tkt, const char **status)
{
    size_t i;
    size_t nrules = sizeof(tgsflagrules) / sizeof(tgsflagrules[0]);
    const struct tgsflagrule *r;

    for (i = 0; i < nrules; i++) {
        r = &tgsflagrules[i];
        if (r->reqflags & req->kdc_options) {
            if (!(r->checkflag & tkt->enc_part2->flags)) {
                *status = r->status;
                return r->err;
            }
        }
    }

    if (isflagset(tkt->enc_part2->flags, TKT_FLG_INVALID) &&
        !isflagset(req->kdc_options, KDC_OPT_VALIDATE)) {
        *status = "TICKET NOT VALID";
        return KRB5KRB_AP_ERR_TKT_NYV;
    }

    return 0;
}

static const struct tgsflagrule svcdenyrules[] = {
    { KDC_OPT_RENEWABLE, KRB5_KDB_DISALLOW_RENEWABLE,
      "NON-RENEWABLE TICKET", KRB5KDC_ERR_POLICY },
    { KDC_OPT_ALLOW_POSTDATE, KRB5_KDB_DISALLOW_POSTDATED,
      "NON-POSTDATABLE TICKET", KRB5KDC_ERR_CANNOT_POSTDATE },
    { KDC_OPT_ENC_TKT_IN_SKEY, KRB5_KDB_DISALLOW_DUP_SKEY,
      "DUP_SKEY DISALLOWED", KRB5KDC_ERR_POLICY }
};

/*
 * A service principal can forbid some TGS-REQ options.
 */
static krb5_error_code
check_tgs_svc_deny_opts(krb5_kdc_req *req, krb5_db_entry *server,
                        krb5_ticket *tkt, krb5_timestamp kdc_time,
                        const char **status)
{
    size_t i;
    size_t nrules = sizeof(svcdenyrules) / sizeof(svcdenyrules[0]);
    const struct tgsflagrule *r;

    for (i = 0; i < nrules; i++) {
        r = &svcdenyrules[i];
        if (!(r->reqflags & req->kdc_options))
            continue;
        if (r->checkflag & server->attributes) {
            *status = r->status;
            return r->err;
        }
    }
    return 0;
}

/*
 * A service principal can deny all TGS-REQs for it.
 */
static krb5_error_code
check_tgs_svc_deny_all(krb5_kdc_req *req, krb5_db_entry *server,
                       krb5_ticket *tkt, krb5_timestamp kdc_time,
                       const char **status)
{
    if (server->attributes & KRB5_KDB_DISALLOW_ALL_TIX) {
        *status = "SERVER LOCKED OUT";
        return KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
    }
    if ((server->attributes & KRB5_KDB_DISALLOW_SVR) &&
        !(req->kdc_options & KDC_OPT_ENC_TKT_IN_SKEY)) {
        *status = "SERVER NOT ALLOWED";
        return KRB5KDC_ERR_MUST_USE_USER2USER;
    }
    if (server->attributes & KRB5_KDB_DISALLOW_TGT_BASED) {
        if (krb5_is_tgs_principal(tkt->server)) {
            *status = "TGT BASED NOT ALLOWED";
            return KRB5KDC_ERR_POLICY;
        }
    }
    return 0;
}

/*
 * A service principal can require certain TGT flags.
 */
static krb5_error_code
check_tgs_svc_reqd_flags(krb5_kdc_req *req, krb5_db_entry *server,
                         krb5_ticket *tkt,
                         krb5_timestamp kdc_time, const char **status)
{
    if (server->attributes & KRB5_KDB_REQUIRES_HW_AUTH) {
        if (!(tkt->enc_part2->flags & TKT_FLG_HW_AUTH)) {
            *status = "NO HW PREAUTH";
            return KRB5KRB_ERR_GENERIC;
        }
    }
    if (server->attributes & KRB5_KDB_REQUIRES_PRE_AUTH) {
        if (!(tkt->enc_part2->flags & TKT_FLG_PRE_AUTH)) {
            *status = "NO PREAUTH";
            return KRB5KRB_ERR_GENERIC;
        }
    }
    return 0;
}

static krb5_error_code
check_tgs_svc_time(krb5_kdc_req *req, krb5_db_entry *server, krb5_ticket *tkt,
                   krb5_timestamp kdc_time, const char **status)
{
    if (server->expiration && ts_after(kdc_time, server->expiration)) {
        *status = "SERVICE EXPIRED";
        return KRB5KDC_ERR_SERVICE_EXP;
    }
    return 0;
}

static krb5_error_code
check_tgs_svc_policy(krb5_kdc_req *req, krb5_db_entry *server,
                     krb5_ticket *tkt, krb5_timestamp kdc_time,
                     const char **status)
{
    int errcode;
    size_t i;
    size_t nfns = sizeof(svc_pol_fns) / sizeof(svc_pol_fns[0]);

    for (i = 0; i < nfns; i++) {
        errcode = svc_pol_fns[i](req, server, tkt, kdc_time, status);
        if (errcode != 0)
            return errcode;
    }
    return 0;
}

/*
 * Check header ticket timestamps against the current time.
 */
static krb5_error_code
check_tgs_times(krb5_kdc_req *req, krb5_ticket_times *times,
                krb5_timestamp kdc_time, const char **status)
{
    krb5_timestamp starttime;

    /* For validating a postdated ticket, check the start time vs. the
       KDC time. */
    if (req->kdc_options & KDC_OPT_VALIDATE) {
        starttime = times->starttime ? times->starttime : times->authtime;
        if (ts_after(starttime, kdc_time)) {
            *status = "NOT_YET_VALID";
            return KRB5KRB_AP_ERR_TKT_NYV;
        }
    }
    /*
     * Check the renew_till time.  The endtime was already
     * been checked in the initial authentication check.
     */
    if ((req->kdc_options & KDC_OPT_RENEW) &&
        ts_after(kdc_time, times->renew_till)) {
        *status = "TKT_EXPIRED";
        return KRB5KRB_AP_ERR_TKT_EXPIRED;
    }
    return 0;
}

/* Check for local user tickets issued by foreign realms.  This check is
 * skipped for S4U2Self requests. */
static krb5_error_code
check_tgs_lineage(krb5_db_entry *server, krb5_ticket *tkt,
                  krb5_boolean is_crossrealm, const char **status)
{
    if (is_crossrealm && data_eq(tkt->enc_part2->client->realm,
                                 server->princ->realm)) {
        *status = "INVALID LINEAGE";
        return KRB5KDC_ERR_POLICY;
    }
    return 0;
}

static krb5_error_code
check_tgs_s4u2self(kdc_realm_t *realm, krb5_kdc_req *req,
                   krb5_db_entry *server, krb5_ticket *tkt, krb5_pac pac,
                   krb5_timestamp kdc_time,
                   krb5_pa_s4u_x509_user *s4u_x509_user, krb5_db_entry *client,
                   krb5_boolean is_crossrealm, krb5_boolean is_referral,
                   const char **status, krb5_pa_data ***e_data)
{
    krb5_context context = realm->realm_context;
    krb5_db_entry empty_server = { 0 };

    /* If the server is local, check that the request is for self. */
    if (!is_referral &&
        !is_client_db_alias(context, server, tkt->enc_part2->client)) {
        *status = "INVALID_S4U2SELF_REQUEST_SERVER_MISMATCH";
        return KRB5KRB_AP_ERR_BADMATCH;
    }

    /* S4U2Self requests must use options valid for AS requests. */
    if (req->kdc_options & AS_INVALID_OPTIONS) {
        *status = "INVALID S4U2SELF OPTIONS";
        return KRB5KDC_ERR_BADOPTION;
    }

    /*
     * Valid S4U2Self requests can occur in the following combinations:
     *
     * (1) local TGT, local user, local server
     * (2) cross TGT, local user, issuing referral
     * (3) cross TGT, non-local user, issuing referral
     * (4) cross TGT, non-local user, local server
     *
     * The first case is for a single-realm S4U2Self scenario; the second,
     * third, and fourth cases are for the initial, intermediate (if any), and
     * final cross-realm requests in a multi-realm scenario.
     */

    if (!is_crossrealm && is_referral) {
        /* This could happen if the requesting server no longer exists, and we
         * found a referral instead.  Treat this as a server lookup failure. */
        *status = "LOOKING_UP_SERVER";
        return KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
    }
    if (client != NULL && is_crossrealm && !is_referral) {
        /* A local server should not need a cross-realm TGT to impersonate
         * a local principal. */
        *status = "NOT_CROSS_REALM_REQUEST";
        return KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN; /* match Windows error */
    }
    if (client == NULL && !is_crossrealm) {
        /*
         * The server is asking to impersonate a principal from another realm,
         * using a local TGT.  It should instead ask that principal's realm and
         * follow referrals back to us.
         */
        *status = "S4U2SELF_CLIENT_NOT_OURS";
        return KRB5KDC_ERR_POLICY; /* match Windows error */
    }
    if (client == NULL && s4u_x509_user->user_id.user->length == 0) {
        /*
         * Only a KDC in the client realm can handle a certificate-only
         * S4U2Self request.  Other KDCs require a principal name and ignore
         * the subject-certificate field.
         */
        *status = "INVALID_XREALM_S4U2SELF_REQUEST";
        return KRB5KDC_ERR_POLICY; /* match Windows error */
    }

    /* The header ticket PAC must be present. */
    if (pac == NULL) {
        *status = "S4U2SELF_NO_PAC";
        return KRB5KDC_ERR_TGT_REVOKED;
    }

    if (client != NULL) {
        /* The header ticket PAC must be for the impersonator. */
        if (krb5_pac_verify(context, pac, tkt->enc_part2->times.authtime,
                            tkt->enc_part2->client, NULL, NULL) != 0) {
            *status = "S4U2SELF_LOCAL_PAC_CLIENT";
            return KRB5KDC_ERR_BADOPTION;
        }

        /* Validate the client policy.  Use an empty server principal to bypass
         * server policy checks. */
        return validate_as_request(realm, req, client, &empty_server, kdc_time,
                                   status, e_data);
    } else {
        /* The header ticket PAC must be for the subject, with realm. */
        if (krb5_pac_verify_ext(context, pac, tkt->enc_part2->times.authtime,
                                s4u_x509_user->user_id.user, NULL, NULL,
                                TRUE) != 0) {
            *status = "S4U2SELF_FOREIGN_PAC_CLIENT";
            return KRB5KDC_ERR_BADOPTION;
        }
    }

    return 0;
}

/*
 * Validate pac as an S4U2Proxy subject PAC contained within a cross-realm TGT.
 * If target_server is non-null, verify that it matches the PAC proxy target.
 * Return 0 on success, non-zero on failure.
 */
static int
verify_deleg_pac(krb5_context context, krb5_pac pac,
                 krb5_enc_tkt_part *enc_tkt,
                 krb5_const_principal target_server)
{
    krb5_timestamp pac_authtime;
    krb5_data deleg_buf = empty_data();
    krb5_principal princ = NULL;
    struct pac_s4u_delegation_info *di = NULL;
    char *client_str = NULL, *target_str = NULL;
    const char *last_transited;
    int result = -1;

    /* Make sure the PAC client string can be parsed as a principal with
     * realm. */
    if (get_pac_princ_with_realm(context, pac, &princ, &pac_authtime) != 0)
        goto cleanup;
    if (pac_authtime != enc_tkt->times.authtime)
        goto cleanup;

    if (krb5_pac_get_buffer(context, pac, KRB5_PAC_DELEGATION_INFO,
                            &deleg_buf) != 0)
        goto cleanup;

    if (ndr_dec_delegation_info(&deleg_buf, &di) != 0)
        goto cleanup;

    if (target_server != NULL) {
        if (krb5_unparse_name_flags(context, target_server,
                                    KRB5_PRINCIPAL_UNPARSE_DISPLAY |
                                    KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                    &target_str) != 0)
            goto cleanup;
        if (strcmp(target_str, di->proxy_target) != 0)
            goto cleanup;
    }

    /* Check that the most recently added PAC transited service matches the
     * requesting impersonator. */
    if (di->transited_services_length == 0)
        goto cleanup;
    if (krb5_unparse_name(context, enc_tkt->client, &client_str) != 0)
        goto cleanup;
    last_transited = di->transited_services[di->transited_services_length - 1];
    if (strcmp(last_transited, client_str) != 0)
        goto cleanup;

    result = 0;

cleanup:
    free(target_str);
    free(client_str);
    ndr_free_delegation_info(di);
    krb5_free_principal(context, princ);
    krb5_free_data_contents(context, &deleg_buf);
    return result;
}

static krb5_error_code
check_tgs_s4u2proxy(krb5_context context, krb5_kdc_req *req,
                    krb5_db_entry *server, krb5_ticket *tkt, krb5_pac pac,
                    const krb5_ticket *stkt, krb5_pac stkt_pac,
                    krb5_db_entry *stkt_server, krb5_boolean is_crossrealm,
                    krb5_boolean is_referral, const char **status)
{
    /* A forwardable second ticket must be present in the request. */
    if (stkt == NULL) {
        *status = "NO_2ND_TKT";
        return KRB5KDC_ERR_BADOPTION;
    }
    if (!(stkt->enc_part2->flags & TKT_FLG_FORWARDABLE)) {
        *status = "EVIDENCE_TKT_NOT_FORWARDABLE";
        return KRB5KDC_ERR_BADOPTION;
    }

    /* Constrained delegation is mutually exclusive with renew/forward/etc.
     * (and therefore requires the header ticket to be a TGT). */
    if (req->kdc_options & (NON_TGT_OPTION | KDC_OPT_ENC_TKT_IN_SKEY)) {
        *status = "INVALID_S4U2PROXY_OPTIONS";
        return KRB5KDC_ERR_BADOPTION;
    }

    /* Can't get a TGT (otherwise it would be unconstrained delegation). */
    if (krb5_is_tgs_principal(req->server)) {
        *status = "NOT_ALLOWED_TO_DELEGATE";
        return KRB5KDC_ERR_POLICY;
    }

    /* The header ticket PAC must be present and for the impersonator. */
    if (pac == NULL) {
        *status = "S4U2PROXY_NO_HEADER_PAC";
        return KRB5KDC_ERR_TGT_REVOKED;
    }
    if (krb5_pac_verify(context, pac, tkt->enc_part2->times.authtime,
                        tkt->enc_part2->client, NULL, NULL) != 0) {
        *status = "S4U2PROXY_HEADER_PAC";
        return KRB5KDC_ERR_BADOPTION;
    }

    /*
     * An S4U2Proxy request must be an initial request to the impersonator's
     * realm (possibly for a target resource in the same realm), or a final
     * cross-realm RBCD request to the resource realm.  Intermediate
     * referral-chasing requests do not use the CNAME-IN-ADDL-TKT flag.
     */

    if (stkt_pac == NULL) {
        *status = "S4U2PROXY_NO_STKT_PAC";
        return KRB5KRB_AP_ERR_MODIFIED;
    }
    if (!is_crossrealm) {
        /* For an initial or same-realm request, the second ticket server and
         * header ticket client must be the same principal. */
        if (!is_client_db_alias(context, stkt_server,
                                tkt->enc_part2->client)) {
            *status = "EVIDENCE_TICKET_MISMATCH";
            return KRB5KDC_ERR_SERVER_NOMATCH;
        }

        /* The second ticket client and PAC client are the subject, and must
         * match. */
        if (krb5_pac_verify(context, stkt_pac, stkt->enc_part2->times.authtime,
                            stkt->enc_part2->client, NULL, NULL) != 0) {
            *status = "S4U2PROXY_LOCAL_STKT_PAC";
            return KRB5KDC_ERR_BADOPTION;
        }

    } else {

        /*
         * For a cross-realm request, the second ticket must be a referral TGT
         * to our realm with the impersonator as client.  The target server
         * must also be local, so we must not be issuing a referral.
         */
        if (is_referral || !is_cross_tgs_principal(stkt_server->princ) ||
            !data_eq(stkt_server->princ->data[1], server->princ->realm) ||
            !krb5_principal_compare(context, stkt->enc_part2->client,
                                    tkt->enc_part2->client)) {
            *status = "XREALM_EVIDENCE_TICKET_MISMATCH";
            return KRB5KDC_ERR_BADOPTION;
        }

        /* The second ticket PAC must be present and for the impersonated
         * client, with delegation info. */
        if (stkt_pac == NULL ||
            verify_deleg_pac(context, stkt_pac, stkt->enc_part2,
                             req->server) != 0) {
            *status = "S4U2PROXY_CROSS_STKT_PAC";
            return KRB5KDC_ERR_BADOPTION;
        }
    }

    return 0;
}

/* Check the KDB policy for a final RBCD request. */
static krb5_error_code
check_s4u2proxy_policy(krb5_context context, krb5_kdc_req *req,
                       krb5_principal desired_client,
                       krb5_principal impersonator_name,
                       krb5_db_entry *impersonator, krb5_pac impersonator_pac,
                       krb5_principal resource_name, krb5_db_entry *resource,
                       krb5_boolean is_crossrealm, krb5_boolean is_referral,
                       const char **status)
{
    krb5_error_code ret;
    krb5_boolean support_rbcd, policy_denial = FALSE;

    /* Check if the client supports resource-based constrained delegation. */
    ret = kdc_get_pa_pac_rbcd(context, req->padata, &support_rbcd);
    if (ret)
        return ret;

    if (is_referral) {
        if (!support_rbcd) {
            /* The client must support RBCD for a referral to be useful. */
            *status = "UNSUPPORTED_S4U2PROXY_REQUEST";
            return KRB5KDC_ERR_BADOPTION;
        }
        /* Policy will be checked in the resource realm. */
        return 0;
    }

    /* Try resource-based authorization if the client supports RBCD. */
    if (support_rbcd) {
        ret = krb5_db_allowed_to_delegate_from(context, desired_client,
                                               impersonator_name,
                                               impersonator_pac, resource);
        if (ret == KRB5KDC_ERR_BADOPTION)
            policy_denial = TRUE;
        else if (ret != KRB5_PLUGIN_OP_NOTSUPP)
            return ret;
    }

    /* Try traditional authorization if the requestor is in this realm. */
    if (!is_crossrealm) {
        ret = krb5_db_check_allowed_to_delegate(context, desired_client,
                                                impersonator, resource_name);
        if (ret == KRB5KDC_ERR_BADOPTION)
            policy_denial = TRUE;
        else if (ret != KRB5_PLUGIN_OP_NOTSUPP)
            return ret;
    }

    *status = policy_denial ? "NOT_ALLOWED_TO_DELEGATE" :
        "UNSUPPORTED_S4U2PROXY_REQUEST";
    return KRB5KDC_ERR_BADOPTION;
}

static krb5_error_code
check_tgs_u2u(krb5_context context, krb5_kdc_req *req, const krb5_ticket *stkt,
              krb5_db_entry *server, const char **status)
{
    /* A second ticket must be present in the request. */
    if (stkt == NULL) {
        *status = "NO_2ND_TKT";
        return KRB5KDC_ERR_BADOPTION;
    }

    /* The second ticket must be a TGT to the server realm. */
    if (!is_local_tgs_principal(stkt->server) ||
        !data_eq(stkt->server->data[1], server->princ->realm)) {
        *status = "2ND_TKT_NOT_TGS";
        return KRB5KDC_ERR_POLICY;
    }

    /* The second ticket client must match the requested server. */
    if (!is_client_db_alias(context, server, stkt->enc_part2->client)) {
        *status = "2ND_TKT_MISMATCH";
        return KRB5KDC_ERR_SERVER_NOMATCH;
    }

    return 0;
}

/* Validate the PAC of a non-S4U TGS request, if one is present. */
static krb5_error_code
check_normal_tgs_pac(krb5_context context, krb5_enc_tkt_part *enc_tkt,
                     krb5_pac pac, krb5_db_entry *server,
                     krb5_boolean is_crossrealm, const char **status)
{
    /* We don't require a PAC for regular TGS requests. */
    if (pac == NULL)
        return 0;

    /* For most requests the header ticket PAC will be for the ticket
     * client. */
    if (krb5_pac_verify(context, pac, enc_tkt->times.authtime, enc_tkt->client,
                        NULL, NULL) == 0)
        return 0;

    /* For intermediate RBCD requests the header ticket PAC will be for the
     * impersonated client. */
    if (is_crossrealm && is_cross_tgs_principal(server->princ) &&
        verify_deleg_pac(context, pac, enc_tkt, NULL) == 0)
        return 0;

    *status = "HEADER_PAC";
    return KRB5KDC_ERR_BADOPTION;
}

/*
 * Some TGS-REQ options allow for a non-TGS principal in the ticket.  Do some
 * checks that are peculiar to these cases.  (e.g., ticket service principal
 * matches requested service principal)
 */
static krb5_error_code
check_tgs_nontgt(krb5_context context, krb5_kdc_req *req, krb5_ticket *tkt,
                 const char **status)
{
    if (!krb5_principal_compare(context, tkt->server, req->server)) {
        *status = "SERVER DIDN'T MATCH TICKET FOR RENEW/FORWARD/ETC";
        return KRB5KDC_ERR_SERVER_NOMATCH;
    }
    /* Cannot proxy ticket granting tickets. */
    if ((req->kdc_options & KDC_OPT_PROXY) &&
        krb5_is_tgs_principal(req->server)) {
        *status = "CAN'T PROXY TGT";
        return KRB5KDC_ERR_BADOPTION;
    }
    return 0;
}

/*
 * Do some checks for a normal TGS-REQ (where the ticket service must be a TGS
 * principal).
 */
static krb5_error_code
check_tgs_tgt(krb5_kdc_req *req, krb5_ticket *tkt, const char **status)
{
    /* Make sure it's a TGS principal. */
    if (!krb5_is_tgs_principal(tkt->server)) {
        *status = "BAD TGS SERVER NAME";
        return KRB5KRB_AP_ERR_NOT_US;
    }
    /* TGS principal second component must match service realm. */
    if (!data_eq(tkt->server->data[1], req->server->realm)) {
        *status = "BAD TGS SERVER INSTANCE";
        return KRB5KRB_AP_ERR_NOT_US;
    }
    return 0;
}

krb5_error_code
check_tgs_constraints(kdc_realm_t *realm, krb5_kdc_req *request,
                      krb5_db_entry *server, krb5_ticket *ticket, krb5_pac pac,
                      const krb5_ticket *stkt, krb5_pac stkt_pac,
                      krb5_db_entry *stkt_server, krb5_timestamp kdc_time,
                      krb5_pa_s4u_x509_user *s4u_x509_user,
                      krb5_db_entry *s4u2self_client,
                      krb5_boolean is_crossrealm, krb5_boolean is_referral,
                      const char **status, krb5_pa_data ***e_data)
{
    krb5_context context = realm->realm_context;
    int errcode;

    /* Depends only on request and ticket. */
    errcode = check_tgs_opts(request, ticket, status);
    if (errcode != 0)
        return errcode;

    /* Depends only on request, ticket times, and current time. */
    errcode = check_tgs_times(request, &ticket->enc_part2->times, kdc_time,
                              status);
    if (errcode != 0)
        return errcode;

    if (request->kdc_options & NON_TGT_OPTION)
        errcode = check_tgs_nontgt(context, request, ticket, status);
    else
        errcode = check_tgs_tgt(request, ticket, status);
    if (errcode != 0)
        return errcode;

    if (s4u_x509_user != NULL) {
        errcode = check_tgs_s4u2self(realm, request, server, ticket, pac,
                                     kdc_time, s4u_x509_user, s4u2self_client,
                                     is_crossrealm, is_referral, status,
                                     e_data);
    } else {
        errcode = check_tgs_lineage(server, ticket, is_crossrealm, status);
    }
    if (errcode != 0)
        return errcode;

    if (request->kdc_options & KDC_OPT_ENC_TKT_IN_SKEY) {
        errcode = check_tgs_u2u(context, request, stkt, server, status);
        if (errcode != 0)
            return errcode;
    }

    if (request->kdc_options & KDC_OPT_CNAME_IN_ADDL_TKT) {
        errcode = check_tgs_s4u2proxy(context, request, server, ticket, pac,
                                      stkt, stkt_pac, stkt_server,
                                      is_crossrealm, is_referral, status);
        if (errcode != 0)
            return errcode;
    } else if (s4u_x509_user == NULL) {
        errcode = check_normal_tgs_pac(context, ticket->enc_part2, pac, server,
                                       is_crossrealm, status);
        if (errcode != 0)
            return errcode;
    }

    return 0;
}

krb5_error_code
check_tgs_policy(kdc_realm_t *realm, krb5_kdc_req *request,
                 krb5_db_entry *server, krb5_ticket *ticket,
                 krb5_pac pac, const krb5_ticket *stkt, krb5_pac stkt_pac,
                 krb5_principal stkt_pac_client, krb5_db_entry *stkt_server,
                 krb5_timestamp kdc_time, krb5_boolean is_crossrealm,
                 krb5_boolean is_referral, const char **status,
                 krb5_pa_data ***e_data)
{
    krb5_context context = realm->realm_context;
    int errcode;
    krb5_error_code ret;
    krb5_principal desired_client;

    errcode = check_tgs_svc_policy(request, server, ticket, kdc_time, status);
    if (errcode != 0)
        return errcode;

    if (request->kdc_options & KDC_OPT_CNAME_IN_ADDL_TKT) {
        desired_client = (stkt_pac_client != NULL) ? stkt_pac_client :
            stkt->enc_part2->client;
        errcode = check_s4u2proxy_policy(context, request, desired_client,
                                         ticket->enc_part2->client,
                                         stkt_server, pac, request->server,
                                         server, is_crossrealm, is_referral,
                                         status);
        if (errcode != 0)
            return errcode;
    }

    if (check_anon(realm, ticket->enc_part2->client, request->server) != 0) {
        *status = "ANONYMOUS NOT ALLOWED";
        return KRB5KDC_ERR_POLICY;
    }

    /* Perform KDB module policy checks. */
    ret = krb5_db_check_policy_tgs(context, request, server, ticket, status,
                                   e_data);
    return (ret == KRB5_PLUGIN_OP_NOTSUPP) ? 0 : ret;
}
