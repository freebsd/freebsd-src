/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/audit/kdc_j_encode.c - Utilities to json encode KDC audit stuff */
/*
 * Copyright (C) 2013 by the Massachusetts Institute of Technology.
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

#include <k5-int.h>
#include <k5-json.h>
#include "kdc_j_encode.h"
#include "j_dict.h"
#include <krb5/audit_plugin.h>
#include <syslog.h>

static krb5_error_code
string_to_value(const char *in, k5_json_object obj, const char *key);
static krb5_error_code
princ_to_value(krb5_principal princ, k5_json_object obj, const char *key);
static krb5_error_code
data_to_value(krb5_data *data, k5_json_object obj, const char *key);
static krb5_error_code
int32_to_value(krb5_int32 int32, k5_json_object obj, const char *key);
static krb5_error_code
bool_to_value(krb5_boolean b, k5_json_object obj, const char *key);
static krb5_error_code
addr_to_obj(krb5_address *a, k5_json_object obj);
static krb5_error_code
eventinfo_to_value(k5_json_object obj, const char *name,
                   const int stage, const krb5_boolean ev_success);
static krb5_error_code
addr_to_value(const krb5_address *address, k5_json_object obj,
              const char *key);
static krb5_error_code
req_to_value(krb5_kdc_req *req, const krb5_boolean ev_success,
             k5_json_object obj);
static krb5_error_code
rep_to_value(krb5_kdc_rep *rep, const krb5_boolean ev_success,
             k5_json_object obj);
static krb5_error_code
tkt_to_value(krb5_ticket *tkt, k5_json_object obj, const char *key);
static char *map_patype(krb5_preauthtype pa_type);

#define NULL_STATE "state is NULL"
#define T_RENEWED 1
#define T_NOT_RENEWED 2
#define T_VALIDATED 1
#define T_NOT_VALIDATED 2

/* KDC server STOP. Returns 0 on success. */
krb5_error_code
kau_j_kdc_stop(const krb5_boolean ev_success, char **jout)
{
    krb5_error_code ret = 0;
    k5_json_object obj = NULL;

    *jout = NULL;

    /* Main object. */
    if (k5_json_object_create(&obj))
        return ENOMEM;

    /* Audit event_ID and ev_success. */
    ret = string_to_value("KDC_STOP", obj, AU_EVENT_NAME);
    if (!ret)
        ret = bool_to_value(ev_success, obj, AU_EVENT_STATUS);
    if (!ret)
        ret = k5_json_encode(obj, jout);
    k5_json_release(obj);

    return ret;
}

/* KDC server START. Returns 0 on success. */
krb5_error_code
kau_j_kdc_start(const krb5_boolean ev_success, char **jout)
{
    krb5_error_code ret = 0;
    k5_json_object obj = NULL;

    *jout = NULL;

    /* Main object. */
    if (k5_json_object_create(&obj))
        return ENOMEM;

    /* Audit event_ID and ev_success. */
    ret = string_to_value("KDC_START", obj, AU_EVENT_NAME);
    if (!ret)
        ret = bool_to_value(ev_success, obj, AU_EVENT_STATUS);
    if (!ret)
        ret = k5_json_encode(obj, jout);
    k5_json_release(obj);

    return ret;
}

/* AS-REQ. Returns 0 on success. */
krb5_error_code
kau_j_as_req(const krb5_boolean ev_success, krb5_audit_state *state,
             char **jout)
{
    krb5_error_code ret = 0;
    k5_json_object obj = NULL;

    *jout = NULL;

    if (!state) {
        *jout = NULL_STATE;
        return 0;
    }

    /* Main object. */
    if (k5_json_object_create(&obj))
        return ENOMEM;
    /* Audit event_ID and ev_success. */
    ret = eventinfo_to_value(obj, "AS_REQ", state->stage, ev_success);
    if (ret)
        goto error;
    /* TGT ticket ID */
    ret = string_to_value(state->tkt_out_id, obj, AU_TKT_OUT_ID);
    if (ret)
        goto error;
    /* Request ID. */
    ret = string_to_value(state->req_id, obj, AU_REQ_ID);
    if (ret)
        goto error;
    /* Client's port and address. */
    ret = int32_to_value(state->cl_port, obj, AU_FROMPORT);
    if (ret)
        goto error;
    ret = addr_to_value(state->cl_addr, obj, AU_FROMADDR);
    if (ret)
        goto error;
    /* KDC status msg */
    ret = string_to_value(state->status, obj, AU_KDC_STATUS);
    if (ret)
        goto error;
    /* non-local client's referral realm. */
    ret = data_to_value(state->cl_realm, obj, AU_CREF_REALM);
    if (ret)
        goto error;
    /* Request. */
    ret = req_to_value(state->request, ev_success, obj);
    if (ret == ENOMEM)
        goto error;
    /* Reply/ticket info. */
    ret = rep_to_value(state->reply, ev_success, obj);
    if (ret == ENOMEM)
        goto error;
    ret = k5_json_encode(obj, jout);

error:
    k5_json_release(obj);
    return ret;
}

/* TGS-REQ. Returns 0 on success. */
krb5_error_code
kau_j_tgs_req(const krb5_boolean ev_success, krb5_audit_state *state,
              char **jout)
{
    krb5_error_code ret = 0;
    k5_json_object obj = NULL;
    krb5_kdc_req *req = state->request;
    int tkt_validated = 0, tkt_renewed = 0;

    *jout = NULL;

    if (!state) {
        *jout = NULL_STATE;
        return 0;
    }

    /* Main object. */
    if (k5_json_object_create(&obj))
        return ENOMEM;

    /* Audit Event ID and ev_success. */
    ret = eventinfo_to_value(obj, "TGS_REQ", state->stage, ev_success);
    if (ret)
        goto error;
    /* Primary and derived ticket IDs. */
    ret = string_to_value(state->tkt_in_id, obj, AU_TKT_IN_ID);
    if (ret)
        goto error;
    ret = string_to_value(state->tkt_out_id, obj, AU_TKT_OUT_ID);
    if (ret)
        goto error;
    /* Request ID */
    ret = string_to_value(state->req_id, obj, AU_REQ_ID);
    if (ret)
        goto error;
    /* client’s address and port. */
    ret = int32_to_value(state->cl_port, obj, AU_FROMPORT);
    if (ret)
        goto error;
    ret = addr_to_value(state->cl_addr, obj, AU_FROMADDR);
    if (ret)
        goto error;
    /* Ticket was renewed, validated. */
    if ((ev_success == TRUE) && (req != NULL)) {
        tkt_renewed = (req->kdc_options & KDC_OPT_RENEW) ?
                      T_RENEWED : T_NOT_RENEWED;
        tkt_validated = (req->kdc_options & KDC_OPT_VALIDATE) ?
                      T_VALIDATED : T_NOT_VALIDATED;
    }
    ret = int32_to_value(tkt_renewed, obj, AU_TKT_RENEWED);
    if (ret)
        goto error;
    ret = int32_to_value(tkt_validated, obj, AU_TKT_VALIDATED);
    if (ret)
        goto error;
    /* KDC status msg, including "ISSUE". */
    ret = string_to_value(state->status, obj, AU_KDC_STATUS);
    if (ret)
        goto error;
    /* request */
    ret = req_to_value(req, ev_success, obj);
    if (ret == ENOMEM)
        goto error;
    /* reply/ticket */
    ret = rep_to_value(state->reply, ev_success, obj);
    if (ret == ENOMEM)
        goto error;
    ret = k5_json_encode(obj, jout);

error:
    k5_json_release(obj);
    return ret;
}

/* S4U2Self protocol extension. Returns 0 on success. */
krb5_error_code
kau_j_tgs_s4u2self(const krb5_boolean ev_success, krb5_audit_state *state,
                   char **jout)
{
    krb5_error_code ret = 0;
    k5_json_object obj = NULL;

    *jout = NULL;

    if (!state) {
        *jout = NULL_STATE;
        return 0;
    }

    /* Main object. */
    if (k5_json_object_create(&obj))
        return ENOMEM;

    /* Audit Event ID and ev_success. */
    ret = eventinfo_to_value(obj, "S4U2SELF", state->stage, ev_success);
    if (ret)
        goto error;
    /* Front-end server's TGT ticket ID. */
    ret = string_to_value(state->tkt_in_id, obj, AU_TKT_IN_ID);
    if (ret)
        goto error;
    /* service "to self" ticket or referral TGT ticket ID. */
    ret = string_to_value(state->tkt_out_id, obj, AU_TKT_OUT_ID);
    if (ret)
        goto error;
    /* Request ID. */
    ret = string_to_value(state->req_id, obj, AU_REQ_ID);
    if (ret)
        goto error;
    if (ev_success == FALSE) {
        /* KDC status msg. */
        ret = string_to_value(state->status, obj, AU_KDC_STATUS);
        if (ret)
            goto error;
        /* Local policy or S4U protocol constraints. */
        ret = int32_to_value(state->violation, obj, AU_VIOLATION);
        if (ret)
            goto error;
    }
    /* Impersonated user. */
    ret = princ_to_value(state->s4u2self_user, obj, AU_REQ_S4U2S_USER);
    if (ret)
        goto error;

    ret = k5_json_encode(obj, jout);

error:
    k5_json_release(obj);
    return ret;
}

/* S4U2Proxy protocol extension. Returns 0 on success. */
krb5_error_code
kau_j_tgs_s4u2proxy(const krb5_boolean ev_success, krb5_audit_state *state,
                    char **jout)
{
    krb5_error_code ret = 0;
    k5_json_object obj = NULL;
    krb5_kdc_req *req = state->request;

    *jout = NULL;

    if (!state) {
        *jout = NULL_STATE;
        return 0;
    }

    /* Main object. */
    if (k5_json_object_create(&obj))
        return ENOMEM;

    /* Audit Event ID and ev_success. */
    ret = eventinfo_to_value(obj, "S4U2PROXY", state->stage, ev_success);
    if (ret)
        goto error;
    /* Front-end server's TGT ticket ID. */
    ret = string_to_value(state->tkt_in_id, obj, AU_TKT_IN_ID);
    if (ret)
        goto error;
    /* Resource service or referral TGT ticket ID. */
    ret = string_to_value(state->tkt_out_id, obj, AU_TKT_OUT_ID);
    if (ret)
        goto error;
    /* User's evidence ticket ID. */
    ret = string_to_value(state->evid_tkt_id, obj, AU_EVIDENCE_TKT_ID);
    if (ret)
        goto error;
    /* Request ID. */
    ret = string_to_value(state->req_id, obj, AU_REQ_ID);
    if (ret)
        goto error;

    if (ev_success == FALSE) {
        /* KDC status msg. */
        ret = string_to_value(state->status, obj, AU_KDC_STATUS);
        if (ret)
            goto error;
        /* Local policy or S4U protocol constraints. */
        ret = int32_to_value(state->violation, obj, AU_VIOLATION);
        if (ret)
            goto error;
    }
    /* Delegated user. */
    if (req != NULL) {
        ret = princ_to_value(req->second_ticket[0]->enc_part2->client,
                             obj, AU_REQ_S4U2P_USER);
        if (ret)
            goto error;
    }
    ret = k5_json_encode(obj, jout);

error:
    k5_json_release(obj);
    return ret;
}

/* U2U. Returns 0 on success. */
krb5_error_code
kau_j_tgs_u2u(const krb5_boolean ev_success, krb5_audit_state *state,
              char **jout)
{
    krb5_error_code ret = 0;
    k5_json_object obj = NULL;
    krb5_kdc_req *req = state->request;

    if (!state) {
        *jout = NULL_STATE;
        return 0;
    }

    *jout = NULL;

    /* Main object. */
    if (k5_json_object_create(&obj))
        return ENOMEM;
    /* Audit Event ID and ev_success. */
    ret = eventinfo_to_value(obj, "U2U", state->stage, ev_success);
    if (ret)
        goto error;
    /* Front-end server's TGT ticket ID. */
    ret = string_to_value(state->tkt_in_id, obj, AU_TKT_IN_ID);
    if (ret)
        goto error;
    /* Service ticket ID. */
    ret = string_to_value(state->tkt_out_id, obj, AU_TKT_OUT_ID);
    if (ret)
        goto error;
    /* Request ID. */
    ret = string_to_value(state->req_id, obj, AU_REQ_ID);
    if (ret)
        goto error;

    if (ev_success == FALSE) {
        /* KDC status msg. */
        ret = string_to_value(state->status, obj, AU_KDC_STATUS);
        if (ret)
            goto error;
    }
    /* Client in the second ticket. */
    if (req != NULL) {
        ret = princ_to_value(req->second_ticket[0]->enc_part2->client,
                             obj, AU_REQ_U2U_USER);
        if (ret)
            goto error;
    }
    /* Enctype of a session key of the second ticket. */
    ret = int32_to_value(req->second_ticket[0]->enc_part2->session->enctype,
                         obj, AU_SRV_ETYPE);
    if (ret)
        goto error;

    ret = k5_json_encode(obj, jout);

error:
    k5_json_release(obj);
    return ret;
}

/* Low level utilities */

/* Converts string into a property of a JSON object. Returns 0 on success.*/
static krb5_error_code
string_to_value(const char *in, k5_json_object obj, const char *key)
{
    krb5_error_code ret = 0;
    k5_json_string str = NULL;

    if (in == NULL)
        return 0;

    ret = k5_json_string_create(in, &str);
    if (ret)
        return ret;
    ret = k5_json_object_set(obj, key, str);
    k5_json_release(str);

    return ret;
}

/*
 * Converts a krb5_data struct into a property of a JSON object.
 * (Borrowed from preauth_otp.c)
 * Returns 0 on success.
 */
static krb5_error_code
data_to_value(krb5_data *data, k5_json_object obj, const char *key)
{
    krb5_error_code ret = 0;
    k5_json_string str = NULL;

    if (data == NULL || data->data == NULL || data->length < 1)
        return 0;

    ret = k5_json_string_create_len(data->data, data->length, &str);
    if (ret)
        return ret;
    ret = k5_json_object_set(obj, key, str);
    k5_json_release(str);

    return ret;
}

/*
 * Converts krb5_int32 into a property of a JSON object.
 * Returns 0 on success.
 */
static krb5_error_code
int32_to_value(krb5_int32 int32, k5_json_object obj, const char *key)
{
    krb5_error_code ret = 0;
    k5_json_number num = NULL;

    ret = k5_json_number_create(int32, &num);
    if (ret)
        return ENOMEM;
    ret = k5_json_object_set(obj, key, num);
    k5_json_release(num);

    return ret;
}

/*
 * Converts krb5_boolean into a property of a JSON object.
 * Returns 0 on success.
 */
static krb5_error_code
bool_to_value(krb5_boolean in, k5_json_object obj, const char *key)
{
    krb5_error_code ret = 0;
    k5_json_bool b = 0;

    ret = k5_json_bool_create(in, &b);
    if (ret)
        return ENOMEM;

    ret = k5_json_object_set(obj, key, b);
    k5_json_release(b);

    return ret;
}

/* Wrapper-level utilities */

/* Wrapper for stage and event_status tags. Returns 0 on success. */
static krb5_error_code
eventinfo_to_value(k5_json_object obj, const char *name,
                   const int stage, const krb5_boolean ev_success)
{
    krb5_error_code ret = 0;

    ret = string_to_value(name, obj, AU_EVENT_NAME);
    if (ret)
        return ret;
    ret = int32_to_value(stage, obj, AU_STAGE);
    if (!ret)
        ret = bool_to_value(ev_success, obj, AU_EVENT_STATUS);

    return ret;
}

/*
 * Converts krb5_principal into a property of a JSON object.
 * Returns 0 on success.
 */
static krb5_error_code
princ_to_value(krb5_principal princ, k5_json_object obj, const char *key)
{
    krb5_error_code ret = 0;
    k5_json_object tmp = NULL;
    k5_json_array arr = NULL;
    k5_json_string str = NULL;
    int i = 0;

    if (princ == NULL || princ->data == NULL)
        return 0;

    /* Main object. */
    if (k5_json_object_create(&tmp))
        return ENOMEM;

    ret = k5_json_array_create(&arr);
    if (ret)
        goto error;
    for (i = 0; i < princ->length; i++) {
        ret = k5_json_string_create_len((&princ->data[i])->data,
                                       (&princ->data[i])->length, &str);
        if (ret)
            goto error;
        ret = k5_json_array_add(arr, str);
        k5_json_release(str);
        if (ret)
            goto error;
    }
    ret = k5_json_object_set(tmp, AU_COMPONENTS, arr);
    if (ret)
        goto error;
    ret = data_to_value(&princ->realm, tmp, AU_REALM);
    if (ret)
        goto error;
    ret = int32_to_value(princ->length, tmp, AU_LENGTH);
    if (ret)
        goto error;
    ret = int32_to_value(princ->type, tmp, AU_TYPE);
    if (ret)
        goto error;

    ret = k5_json_object_set(obj, key, tmp);

error:
    k5_json_release(tmp);
    k5_json_release(arr);
    return ret;
}

/*
 * Helper for JSON encoding of krb5_address.
 * Returns 0 on success.
 */
static krb5_error_code
addr_to_obj(krb5_address *a, k5_json_object obj)
{
    krb5_error_code ret = 0;
    k5_json_number num = NULL;
    k5_json_array arr = NULL;
    int i;

    if (a == NULL || a->contents == NULL || a->length <= 0)
        return 0;

    ret = int32_to_value(a->addrtype, obj, AU_TYPE);
    if (ret)
        goto error;
    ret = int32_to_value(a->length, obj, AU_LENGTH);
    if (ret)
        goto error;

    if (a->addrtype == ADDRTYPE_INET || a->addrtype == ADDRTYPE_INET6) {
        ret = k5_json_array_create(&arr);
        if (ret)
            goto error;
        for (i = 0; i < (int)a->length; i++) {
            ret = k5_json_number_create(a->contents[i], &num);
            if (ret)
                goto error;
            ret = k5_json_array_add(arr, num);
            k5_json_release(num);
            if (ret)
                goto error;
        }
        ret = k5_json_object_set(obj, AU_IP, arr);
        if (ret)
            goto error;
    }

error:
    k5_json_release(arr);
    return ret;
}

/*
 * Converts krb5_fulladdr into a property of a JSON object.
 * Returns 0 on success.
 */
static krb5_error_code
addr_to_value(const krb5_address *address, k5_json_object obj, const char *key)
{
    krb5_error_code ret = 0;
    k5_json_object addr_obj = NULL;

    if (address == NULL)
        return 0;

    ret = k5_json_object_create(&addr_obj);
    if (ret)
        return ret;
    ret = addr_to_obj((krb5_address *)address, addr_obj);
    if (!ret)
        ret = k5_json_object_set(obj, key, addr_obj);
    k5_json_release(addr_obj);

    return ret;
}

/*
 * Helper for JSON encoding of krb5_kdc_req.
 * Returns 0 on success.
 */
static krb5_error_code
req_to_value(krb5_kdc_req *req, const krb5_boolean ev_success,
             k5_json_object obj)
{
    krb5_error_code ret = 0;
    k5_json_number num = NULL;
    k5_json_string str = NULL;
    k5_json_object tmpa = NULL;
    k5_json_array arr = NULL, arra = NULL, arrpa = NULL;
    krb5_pa_data **padata;
    int i = 0;

    if (req == NULL)
        return 0;

    ret = princ_to_value(req->client, obj, AU_REQ_CLIENT);
    if (ret)
        goto error;
    ret = princ_to_value(req->server, obj, AU_REQ_SERVER);
    if (ret)
        goto error;

    ret = int32_to_value(req->kdc_options, obj, AU_REQ_KDC_OPTIONS);
        if (ret)
            goto error;
    ret = int32_to_value(req->from, obj, AU_REQ_TKT_START);
        if (ret)
            goto error;
    ret = int32_to_value(req->till, obj, AU_REQ_TKT_END);
        if (ret)
            goto error;
    ret = int32_to_value(req->rtime, obj, AU_REQ_TKT_RENEW_TILL);
        if (ret)
            goto error;
    /* Available/requested enctypes. */
    ret = k5_json_array_create(&arr);
    if (ret)
        goto error;
    for (i = 0; (i < req->nktypes); i++) {
        if (req->ktype[i] > 0) {
            ret = k5_json_number_create(req->ktype[i], &num);
            if (ret)
                goto error;
            ret = k5_json_array_add(arr, num);
            k5_json_release(num);
            if (ret)
                goto error;
        }
    }
    ret = k5_json_object_set(obj, AU_REQ_AVAIL_ETYPES, arr);
    if (ret)
        goto error;
    /* Pre-auth types. */
    if (ev_success == TRUE && req->padata) {
            ret = k5_json_array_create(&arrpa);
            if (ret)
                goto error;
            for (padata = req->padata; *padata; padata++) {
                if (strlen(map_patype((*padata)->pa_type)) > 1) {
                    ret = k5_json_string_create(map_patype((*padata)->pa_type),
                                                &str);
                    if (ret)
                        goto error;
                    ret = k5_json_array_add(arrpa, str);
                    k5_json_release(str);
                    if (ret)
                        goto error;
                }
            }
            ret = k5_json_object_set(obj, AU_REQ_PA_TYPE, arrpa);
    }
    /* List of requested addresses. */
    if (req->addresses) {
        ret = k5_json_array_create(&arra);
        if (ret)
                goto error;
        for (i = 0; req->addresses[i] != NULL; i++) {
            ret = k5_json_object_create(&tmpa);
            if (ret)
                goto error;
            ret = addr_to_obj(req->addresses[i], tmpa);
            if (!ret)
                ret = k5_json_array_add(arra, tmpa);
            k5_json_release(tmpa);
            if (ret)
                goto error;
        }
        ret = k5_json_object_set(obj, AU_REQ_ADDRESSES, arra);
        if (ret)
            goto error;
    }
error:
    k5_json_release(arr);
    k5_json_release(arra);
    k5_json_release(arrpa);
    return ret;
}

/*
 * Helper for JSON encoding of krb5_kdc_rep.
 * Returns 0 on success.
 */
static krb5_error_code
rep_to_value(krb5_kdc_rep *rep, const krb5_boolean ev_success,
             k5_json_object obj)
{
    krb5_error_code ret = 0;
    krb5_pa_data **padata;
    k5_json_array arrpa = NULL;
    k5_json_string str = NULL;

    if (rep == NULL)
        return 0;

    if (ev_success == TRUE) {
        ret = tkt_to_value(rep->ticket, obj, AU_REP_TICKET);
        /* Enctype of the reply-encrypting key. */
        ret = int32_to_value(rep->enc_part.enctype, obj, AU_REP_ETYPE);
        if (ret)
            goto error;
    } else {

        if (rep->padata) {
            ret = k5_json_array_create(&arrpa);
            if (ret)
                goto error;
            for (padata = rep->padata; *padata; padata++) {
                if (strlen(map_patype((*padata)->pa_type)) > 1) {
                    ret = k5_json_string_create(map_patype((*padata)->pa_type),
                                                          &str);
                    if (ret)
                        goto error;
                    ret = k5_json_array_add(arrpa, str);
                    k5_json_release(str);
                    if (ret)
                        goto error;
                }
            }
        }
        ret = k5_json_object_set(obj, AU_REP_PA_TYPE, arrpa);
    }
error:
    k5_json_release(arrpa);
    return ret;
}

/*
 * Converts krb5_ticket into a property of a JSON object.
 * Returns 0 on success.
 */
static krb5_error_code
tkt_to_value(krb5_ticket *tkt, k5_json_object obj,
              const char *key)
{
    krb5_error_code ret = 0;
    k5_json_object tmp = NULL;
    krb5_enc_tkt_part *part2 = NULL;

    if (tkt == NULL)
        return 0;

    /* Main object. */
    if (k5_json_object_create(&tmp))
        return ENOMEM;

    /*
     * CNAME - potentially redundant data...
     * ...but it is part of the ticket. So, record it as such.
     */
    ret = princ_to_value(tkt->server, tmp, AU_CNAME);
    if (ret)
        goto error;
    ret = princ_to_value(tkt->server, tmp, AU_SNAME);
    if (ret)
        goto error;
    /* Enctype of a long-term key of service. */
    if (tkt->enc_part.enctype)
        ret = int32_to_value(tkt->enc_part.enctype, tmp, AU_SRV_ETYPE);
    if (ret)
        goto error;
    if (tkt->enc_part2)
        part2 = tkt->enc_part2;
    if (part2) {
        ret = princ_to_value(part2->client, tmp, AU_CNAME);
        if (ret)
            goto error;
        ret = int32_to_value(part2->flags, tmp, AU_FLAGS);
        if (ret)
            goto error;
        /* Chosen by KDC session key enctype (short-term key). */
        ret = int32_to_value(part2->session->enctype, tmp, AU_SESS_ETYPE);
        if (ret)
            goto error;
        ret = int32_to_value(part2->times.starttime, tmp, AU_START);
        if (ret)
            goto error;
        ret = int32_to_value(part2->times.endtime, tmp, AU_END);
        if (ret)
            goto error;
        ret = int32_to_value(part2->times.renew_till, tmp, AU_RENEW_TILL);
        if (ret)
            goto error;
        ret = int32_to_value(part2->times.authtime, tmp, AU_AUTHTIME);
        if (ret)
            goto error;
        if (part2->transited.tr_contents.length > 0) {
            ret = data_to_value(&part2->transited.tr_contents,
                               tmp, AU_TR_CONTENTS);
            if (ret)
                goto error;
        }
    } /* part2 != NULL */

    if (!ret)
        ret = k5_json_object_set(obj, key, tmp);

error:
    k5_json_release(tmp);
    return ret;
}

/* Map preauth numeric type to the naming string. */
struct _patype_str {
    krb5_preauthtype id;
    char *name;
};
struct _patype_str  patype_str[] = {
    {KRB5_PADATA_ENC_TIMESTAMP, "ENC_TIMESTAMP"},
    {KRB5_PADATA_PW_SALT, "PW_SALT"},
    {KRB5_PADATA_ENC_UNIX_TIME, "ENC_UNIX_TIME"},
    {KRB5_PADATA_SAM_CHALLENGE, "SAM_CHALLENGE"},
    {KRB5_PADATA_SAM_RESPONSE, "SAM_RESPONSE"},
    {KRB5_PADATA_PK_AS_REQ_OLD, "PK_AS_REQ_OLD"},
    {KRB5_PADATA_PK_AS_REP_OLD, "PK_AS_REP_OLD"},
    {KRB5_PADATA_PK_AS_REQ, "PK_AS_REQ"},
    {KRB5_PADATA_PK_AS_REP, "PK_AS_REP"},
    {KRB5_PADATA_ETYPE_INFO2, "ETYPE_INFO2"},
    {KRB5_PADATA_SAM_CHALLENGE_2, "SAM_CHALLENGE_2"},
    {KRB5_PADATA_SAM_RESPONSE_2, "SAM_RESPONSE_2"},
    {KRB5_PADATA_PAC_REQUEST, "PAC_REQUEST"},
    {KRB5_PADATA_FOR_USER, "FOR_USER"},
    {KRB5_PADATA_S4U_X509_USER, "S4U_X509_USER"},
    {KRB5_PADATA_ENCRYPTED_CHALLENGE, "ENCRYPTED_CHALLENGE"},
    {KRB5_PADATA_OTP_CHALLENGE, "OTP_CHALLENGE"},
    {KRB5_PADATA_OTP_REQUEST, "OTP_REQUEST"},
    {KRB5_PADATA_OTP_PIN_CHANGE, "OTP_PIN_CHANGE"}
};


static char *
map_patype(krb5_preauthtype pa_type)
{
    int i = 0;
    int n = sizeof(patype_str)/sizeof(patype_str[0]);

    for (i = 0; i < n; i++) {
        if (pa_type == patype_str[i].id)
            return patype_str[i].name;
    }
    return "";
}
