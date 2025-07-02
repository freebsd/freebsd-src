/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* j_dict.h - Dictionary file for json implementation of audit system */
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

#ifndef KRB5_J_DICT_H_INCLUDED
#define KRB5_J_DICT_H_INCLUDED

/* Dictionary for KDC events */
#define AU_STAGE              "stage"
#define AU_EVENT_NAME         "event_name"
#define AU_EVENT_STATUS       "event_success"
#define AU_TKT_IN_ID          "tkt_in_id"
#define AU_TKT_OUT_ID         "tkt_out_id"
#define AU_REQ_ID             "req_id"
#define AU_KDC_STATUS         "kdc_status"
#define AU_FROMPORT           "fromport"
#define AU_FROMADDR           "fromaddr"
#define AU_TYPE               "type" /* used by fromaddr */
#define AU_IP                 "ip" /* used by fromaddr */
#define AU_SESS_ETYPE         "sess_etype"
#define AU_SRV_ETYPE          "srv_etype"
#define AU_REP_ETYPE          "rep_etype"
#define AU_REALM              "realm"
#define AU_LENGTH             "length"
#define AU_COMPONENTS         "components"
#define AU_TKT_RENEWED        "tkt_renewed"
#define AU_TKT_VALIDATED      "tkt_validated"
/* referrals */
#define AU_CREF_REALM         "clreferral_realm"
/* request */
#define AU_REQ_KDC_OPTIONS    "req.kdc_options"
#define AU_REQ_SERVER         "req.server"
#define AU_REQ_CLIENT         "req.client"
#define AU_REQ_AVAIL_ETYPES   "req.avail_etypes"
#define AU_EVIDENCE_TKT       "evidence_tkt"
#define AU_REQ_ADDRESSES      "req.addresses"
#define AU_REQ_TKT_START      "req.tkt_start"
#define AU_REQ_TKT_END        "req.tkt_end"
#define AU_REQ_TKT_RENEW_TILL "req.tkt_renew_till"
#define AU_REQ_PA_TYPE        "req.pa_type"
/* reply */
#define AU_REP_TICKET         "rep.ticket"
#define AU_REP_PA_TYPE        "rep.pa_type"
/* ticket */
#define AU_SNAME              "sname"
#define AU_CNAME              "cname"
#define AU_FLAGS              "flags"
#define AU_START              "start"
#define AU_END                "end"
#define AU_RENEW_TILL         "renew_till"
#define AU_AUTHTIME           "authtime"
#define AU_TR_CONTENTS        "tr_contents"
#define AU_CADDRS             "caddrs"
/* S4U and U2U */
#define AU_VIOLATION       "violation"   /* policy or protocol restrictions */
#define AU_REQ_S4U2S_USER  "s4u2self_user"
#define AU_REQ_S4U2P_USER  "s4u2proxy_user"
#define AU_REQ_U2U_USER    "u2u_user"
#define AU_EVIDENCE_TKT_ID "evidence_tkt_id" /* 2nd ticket in s4u2proxy req */
#endif /* KRB5_J_DICT_H_INCLUDED */
