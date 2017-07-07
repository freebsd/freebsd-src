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
typedef int (check_tgs_svc_pol_fn)(krb5_kdc_req *, krb5_db_entry,
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
    { (KDC_OPT_FORWARDED | KDC_OPT_FORWARDABLE), TKT_FLG_FORWARDABLE,
      "TGT NOT FORWARDABLE", KDC_ERR_BADOPTION },
    { (KDC_OPT_PROXY | KDC_OPT_PROXIABLE), TKT_FLG_PROXIABLE,
      "TGT NOT PROXIABLE", KDC_ERR_BADOPTION },
    { (KDC_OPT_ALLOW_POSTDATE | KDC_OPT_POSTDATED), TKT_FLG_MAY_POSTDATE,
      "TGT NOT POSTDATABLE", KDC_ERR_BADOPTION },
    { KDC_OPT_VALIDATE, TKT_FLG_INVALID,
      "VALIDATE VALID TICKET", KDC_ERR_BADOPTION },
    { KDC_OPT_RENEW, TKT_FLG_RENEWABLE,
      "TICKET NOT RENEWABLE", KDC_ERR_BADOPTION }
};

/*
 * Some TGS-REQ options require that the ticket have corresponding flags set.
 */
static int
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
    return 0;
}

static const struct tgsflagrule svcdenyrules[] = {
    { KDC_OPT_FORWARDABLE, KRB5_KDB_DISALLOW_FORWARDABLE,
      "NON-FORWARDABLE TICKET", KDC_ERR_POLICY },
    { KDC_OPT_RENEWABLE, KRB5_KDB_DISALLOW_RENEWABLE,
      "NON-RENEWABLE TICKET", KDC_ERR_POLICY },
    { KDC_OPT_PROXIABLE, KRB5_KDB_DISALLOW_PROXIABLE,
      "NON-PROXIABLE TICKET", KDC_ERR_POLICY },
    { KDC_OPT_ALLOW_POSTDATE, KRB5_KDB_DISALLOW_POSTDATED,
      "NON-POSTDATABLE TICKET", KDC_ERR_CANNOT_POSTDATE },
    { KDC_OPT_ENC_TKT_IN_SKEY, KRB5_KDB_DISALLOW_DUP_SKEY,
      "DUP_SKEY DISALLOWED", KDC_ERR_POLICY }
};

/*
 * A service principal can forbid some TGS-REQ options.
 */
static int
check_tgs_svc_deny_opts(krb5_kdc_req *req, krb5_db_entry server,
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
        if (r->checkflag & server.attributes) {
            *status = r->status;
            return r->err;
        }
    }
    return 0;
}

/*
 * A service principal can deny all TGS-REQs for it.
 */
static int
check_tgs_svc_deny_all(krb5_kdc_req *req, krb5_db_entry server,
                       krb5_ticket *tkt, krb5_timestamp kdc_time,
                       const char **status)
{
    if (server.attributes & KRB5_KDB_DISALLOW_ALL_TIX) {
        *status = "SERVER LOCKED OUT";
        return KDC_ERR_S_PRINCIPAL_UNKNOWN;
    }
    if (server.attributes & KRB5_KDB_DISALLOW_SVR) {
        *status = "SERVER NOT ALLOWED";
        return KDC_ERR_MUST_USE_USER2USER;
    }
    if (server.attributes & KRB5_KDB_DISALLOW_TGT_BASED) {
        if (krb5_is_tgs_principal(tkt->server)) {
            *status = "TGT BASED NOT ALLOWED";
            return KDC_ERR_POLICY;
        }
    }
    return 0;
}

/*
 * A service principal can require certain TGT flags.
 */
static int
check_tgs_svc_reqd_flags(krb5_kdc_req *req, krb5_db_entry server,
                         krb5_ticket *tkt,
                         krb5_timestamp kdc_time, const char **status)
{
    if (server.attributes & KRB5_KDB_REQUIRES_HW_AUTH) {
        if (!(tkt->enc_part2->flags & TKT_FLG_HW_AUTH)) {
            *status = "NO HW PREAUTH";
            return KRB_ERR_GENERIC;
        }
    }
    if (server.attributes & KRB5_KDB_REQUIRES_PRE_AUTH) {
        if (!(tkt->enc_part2->flags & TKT_FLG_PRE_AUTH)) {
            *status = "NO PREAUTH";
            return KRB_ERR_GENERIC;
        }
    }
    return 0;
}

static int
check_tgs_svc_time(krb5_kdc_req *req, krb5_db_entry server, krb5_ticket *tkt,
                   krb5_timestamp kdc_time, const char **status)
{
    if (server.expiration && server.expiration < kdc_time) {
        *status = "SERVICE EXPIRED";
        return KDC_ERR_SERVICE_EXP;
    }
    return 0;
}

static int
check_tgs_svc_policy(krb5_kdc_req *req, krb5_db_entry server, krb5_ticket *tkt,
                     krb5_timestamp kdc_time, const char **status)
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
static int
check_tgs_times(krb5_kdc_req *req, krb5_ticket_times *times,
                krb5_timestamp kdc_time, const char **status)
{
    krb5_timestamp starttime;

    /* For validating a postdated ticket, check the start time vs. the
       KDC time. */
    if (req->kdc_options & KDC_OPT_VALIDATE) {
        starttime = times->starttime ? times->starttime : times->authtime;
        if (starttime > kdc_time) {
            *status = "NOT_YET_VALID";
            return KRB_AP_ERR_TKT_NYV;
        }
    }
    /*
     * Check the renew_till time.  The endtime was already
     * been checked in the initial authentication check.
     */
    if ((req->kdc_options & KDC_OPT_RENEW) && times->renew_till < kdc_time) {
        *status = "TKT_EXPIRED";
        return KRB_AP_ERR_TKT_EXPIRED;
    }
    return 0;
}

static int
check_tgs_s4u2proxy(kdc_realm_t *kdc_active_realm,
                    krb5_kdc_req *req, const char **status)
{
    if (req->kdc_options & KDC_OPT_CNAME_IN_ADDL_TKT) {
        /* Check that second ticket is in request. */
        if (!req->second_ticket || !req->second_ticket[0]) {
            *status = "NO_2ND_TKT";
            return KDC_ERR_BADOPTION;
        }
    }
    return 0;
}

static int
check_tgs_u2u(kdc_realm_t *kdc_active_realm,
              krb5_kdc_req *req, const char **status)
{
    if (req->kdc_options & KDC_OPT_ENC_TKT_IN_SKEY) {
        /* Check that second ticket is in request. */
        if (!req->second_ticket || !req->second_ticket[0]) {
            *status = "NO_2ND_TKT";
            return KDC_ERR_BADOPTION;
        }
        /* Check that second ticket is a TGT. */
        if (!krb5_principal_compare(kdc_context,
                                    req->second_ticket[0]->server,
                                    tgs_server)) {
            *status = "2ND_TKT_NOT_TGS";
            return KDC_ERR_POLICY;
        }
    }
    return 0;
}

/*
 * Some TGS-REQ options allow for a non-TGS principal in the ticket.  Do some
 * checks that are peculiar to these cases.  (e.g., ticket service principal
 * matches requested service principal)
 */
static int
check_tgs_nontgt(kdc_realm_t *kdc_active_realm,
                 krb5_kdc_req *req, krb5_ticket *tkt, const char **status)
{
    if (!krb5_principal_compare(kdc_context, tkt->server, req->server)) {
        *status = "SERVER DIDN'T MATCH TICKET FOR RENEW/FORWARD/ETC";
        return KDC_ERR_SERVER_NOMATCH;
    }
    /* Cannot proxy ticket granting tickets. */
    if ((req->kdc_options & KDC_OPT_PROXY) &&
        krb5_is_tgs_principal(req->server)) {
        *status = "CAN'T PROXY TGT";
        return KDC_ERR_BADOPTION;
    }
    return 0;
}

/*
 * Do some checks for a normal TGS-REQ (where the ticket service must be a TGS
 * principal).
 */
static int
check_tgs_tgt(kdc_realm_t *kdc_active_realm, krb5_kdc_req *req,
              krb5_ticket *tkt, const char **status)
{
    /* Make sure it's a TGS principal. */
    if (!krb5_is_tgs_principal(tkt->server)) {
        *status = "BAD TGS SERVER NAME";
        return KRB_AP_ERR_NOT_US;
    }
    /* TGS principal second component must match service realm. */
    if (!data_eq(*krb5_princ_component(kdc_context, tkt->server, 1),
                 *krb5_princ_realm(kdc_context, req->server))) {
        *status = "BAD TGS SERVER INSTANCE";
        return KRB_AP_ERR_NOT_US;
    }
    return 0;
}

int
validate_tgs_request(kdc_realm_t *kdc_active_realm,
                     register krb5_kdc_req *request, krb5_db_entry server,
                     krb5_ticket *ticket, krb5_timestamp kdc_time,
                     const char **status, krb5_pa_data ***e_data)
{
    int errcode;
    krb5_error_code ret;

    /* Depends only on request and ticket. */
    errcode = check_tgs_opts(request, ticket, status);
    if (errcode != 0)
        return errcode;

    /* Depends only on request, ticket times, and current time. */
    errcode = check_tgs_times(request, &ticket->enc_part2->times, kdc_time,
                              status);
    if (errcode != 0)
        return errcode;

    errcode = check_tgs_svc_policy(request, server, ticket, kdc_time, status);
    if (errcode != 0)
        return errcode;

    if (request->kdc_options & NON_TGT_OPTION)
        errcode = check_tgs_nontgt(kdc_active_realm, request, ticket, status);
    else
        errcode = check_tgs_tgt(kdc_active_realm, request, ticket, status);
    if (errcode != 0)
        return errcode;

    /* Check the hot list */
    if (check_hot_list(ticket)) {
        *status = "HOT_LIST";
        return(KRB_AP_ERR_REPEAT);
    }

    errcode = check_tgs_u2u(kdc_active_realm, request, status);
    if (errcode != 0)
        return errcode;

    errcode = check_tgs_s4u2proxy(kdc_active_realm, request, status);
    if (errcode != 0)
        return errcode;

    if (check_anon(kdc_active_realm, ticket->enc_part2->client,
                   request->server) != 0) {
        *status = "ANONYMOUS NOT ALLOWED";
        return(KDC_ERR_POLICY);
    }

    /* Perform KDB module policy checks. */
    ret = krb5_db_check_policy_tgs(kdc_context, request, &server,
                                   ticket, status, e_data);
    if (ret && ret != KRB5_PLUGIN_OP_NOTSUPP)
        return errcode_to_protocol(ret);

    /* Check local policy. */
    errcode = against_local_policy_tgs(request, server, ticket,
                                       status, e_data);
    if (errcode)
        return errcode;

    return 0;
}
