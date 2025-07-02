/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/krb5/audit_plugin.h - Audit plugin interface */
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
/*
 * NOTE: This is a private interface and may change incompatibly
 *       between versions.
 */
/*
 * Declarations for KDC audit plugin module implementers.  Audit modules allow
 * the KDC to produce log output or audit records in any desired form.
 *
 * The audit interface has a single supported major version, which is 1.  Major
 * version 1 has a current minor version of 1.  Audit modules should define a
 * function named audit_<modulename>_initvt, matching the signature:
 *
 *   krb5_error_code
 *   audit_modname_initvt(krb5_context context, int maj_ver, int min_ver,
 *                        krb5_plugin_vtable vtable);
 *
 * The initvt function should:
 *
 * - Check that the supplied maj_ver number is supported by the module, or
 *   return KRB5_PLUGIN_VER_NOTSUPP if it is not.
 *
 * - Cast the vtable pointer as appropriate for the interface and maj_ver:
 *   maj_ver == 1: Cast to krb5_audit_vtable
 *
 * - Initialize the methods of the vtable, stopping as appropriate for the
 *   supplied min_ver.  Optional methods may be left uninitialized.
 *
 * Memory for the vtable is allocated by the caller, not by the module.
 */

#ifndef KRB5_AU_PLUGIN_H_INCLUDED
#define KRB5_AU_PLUGIN_H_INCLUDED
#include <krb5/krb5.h>

/** KDC processing steps */
#define AUTHN_REQ_CL 1 /**< Authenticate request and client */
#define SRVC_PRINC   2 /**< Determine service principal */
#define VALIDATE_POL 3 /**< Validate local and protocol policies */
#define ISSUE_TKT    4 /**< Issue ticket */
#define ENCR_REP     5 /**< Encrypt reply */

/** Types of violations */
#define PROT_CONSTRAINT 1 /**< Protocol constraint */
#define LOCAL_POLICY    2 /**< Local policy violation */

#define REQID_LEN 32 /* Size of the alphanumeric request ID */

/** KDC audit state structure and declarations */
typedef struct _krb5_audit_state {
    krb5_kdc_req *request;
    krb5_kdc_rep *reply;
    krb5_address *cl_addr; /**< client address */
    krb5_ui_4 cl_port;     /**< client port */
    int stage;             /**< step in KDC processing */
    const char *status;    /**< KDC status message */
    char *tkt_in_id;       /**< primary (TGT) ticket ID */
    char *tkt_out_id;      /**< derived (service or referral TGT) ticket ID */
    /** for s4u2proxy - evidence ticket ID; for u2u - second ticket ID */
    char *evid_tkt_id;
    char req_id[REQID_LEN];  /**< request ID */
    krb5_data *cl_realm;     /**< referrals: remote client's realm */
    krb5_principal s4u2self_user; /**< impersonated user */
    int violation;           /**< local or protocol policy problem */
} krb5_audit_state;

/** An abstract type for audit module data. */
typedef struct krb5_audit_moddata_st *krb5_audit_moddata;

/*
 * Mandatory:
 * - krb5_audit_open_fn,
 * Open connection to the audit system and initialize audit module data.  If
 * the underlying (OS or third party) audit facility fails to open, no
 * auditable KDC events should be recorded.
 */
typedef krb5_error_code
(*krb5_audit_open_fn)(krb5_audit_moddata *auctx);

/*
 * Mandatory:
 * - krb5_audit_close_fn.
 * Close connection to the underlying audit system.
 */
typedef krb5_error_code
(*krb5_audit_close_fn)(krb5_audit_moddata auctx);

/**
 * Log KDC-start event.
 *
 * @param [in] auctx       Audit context
 * @param [in] ev_success  Success/failure of the event being audited
 *
 * @note Optional.
 *
 * @retval 0 Success; otherwise - Kerberos error codes
 */
typedef krb5_error_code
(*krb5_audit_kdc_start_fn)(krb5_audit_moddata auctx, krb5_boolean ev_success);

/**
 * Log KDC-stop event.
 *
 * @param [in] auctx       Audit context
 * @param [in] ev_success  Success/failure of the event being audited
 *
 * @note Optional.
 *
 * @retval 0 Success; otherwise - Kerberos error codes
 */
typedef krb5_error_code
(*krb5_audit_kdc_stop_fn)(krb5_audit_moddata auctx, krb5_boolean ev_success);

/**
 * Log AS exchange event.
 *
 * @param [in] auctx       Audit context
 * @param [in] ev_success  Success/failure of the event being audited
 * @param [in] state       AS-request related auditable information
 *
 * The @a state provides the following data:
 * - Full information about KDC request, assigned request ID, client address
 *   and port, and stage of the AS exchange
 * - If available, the information about the encryption types of the short- and
 *   long-term keys, non-local client's referral realm, KDC status, the TGT
 *   and its ticket ID
 *
 * @note Optional.
 *
 * @retval 0 Success; otherwise - Kerberos error codes
 */
typedef krb5_error_code
(*krb5_audit_as_req_fn)(krb5_audit_moddata auctx,
                        krb5_boolean ev_success, krb5_audit_state *state);

/**
 * Log TGS exchange event.
 *
 * @param [in] auctx       Audit context
 * @param [in] ev_success  Success/failure of the event being audited
 * @param [in] state       TGS-request related auditable information
 *
 * The @a state provides the following data:
 * - Full information about KDC request, assigned request ID, primary ticket
 *   ID, client address and port, and stage of the TGS exchange
 * - If available, the information about the encryption types of the short- and
 *   long-term keys, KDC status, KDC reply, and the output ticket ID
 *
 * @note Optional.
 *
 * @retval 0 Success; otherwise - Kerberos error codes
 */
typedef krb5_error_code
(*krb5_audit_tgs_req_fn)(krb5_audit_moddata auctx,
                         krb5_boolean ev_success, krb5_audit_state *state);

/**
 * Log S4U2SELF event.
 *
 * @param [in] auctx       Audit context
 * @param [in] ev_success  Report on success or failure
 * @param [in] state       s4u2self related auditable information
 *
 * The @a state provides the following data:
 * - Full information about KDC request, assigned request ID, client address
 *   and port, and stage of the TGS exchange
 * - Requesting server's TGT ID, impersonated user principal name, and service
 *   "to self" ticket or referral TGT ID
 * - If available, KDC status, local policy violation or S4U protocol
 *   constraints
 *
 * @note Optional.
 *
 * @retval 0 Success; otherwise - Kerberos error codes
 */
typedef krb5_error_code
(*krb5_audit_s4u2self_fn)(krb5_audit_moddata auctx,
                          krb5_boolean ev_success, krb5_audit_state *state);

/**
 * Log S4U2PROXY event.
 *
 * @param [in] auctx       Audit context
 * @param [in] ev_success  Report on success or failure
 * @param [in] state       s4u2proxy related auditable information
 *
 * The @a state provides the following data:
 * - Full information about request, assigned request ID, client address and
 *   port, and stage of the TGS exchange
 * - Requesting server's TGT ID, delegated user principal name, and evidence
 *   ticket ID
 * - If available, KDC status, local policy violation or S4U protocol
 *   constraints
 *
 * @note Optional.
 *
 * @retval 0 Success; otherwise - Kerberos error codes
 */
typedef krb5_error_code
(*krb5_audit_s4u2proxy_fn)(krb5_audit_moddata auctx,
                           krb5_boolean ev_success, krb5_audit_state *state);

/**
 * Log U2U event.
 *
 * @param [in] auctx       Audit context
 * @param [in] ev_success  Report on success or failure
 * @param [in] state       user-to-user related auditable information
 *
 * The @a state provides the following data:
 * - Full information about request, assigned request ID, client address and
 *   port, and stage of the TGS exchange,
 * - Requestor's TGT ID, service ticket ID, and client's principal name in the
 *   second ticket
 * - If available, KDC status
 *
 * @note Optional.
 *
 * @retval 0 Success; otherwise - Kerberos error codes
 */
typedef krb5_error_code
(*krb5_audit_u2u_fn)(krb5_audit_moddata auctx,
                     krb5_boolean ev_success, krb5_audit_state *state);

/* vtable declaration */
typedef struct krb5_audit_vtable_st {
    /* Mandatory: name of module. */
    const char             *name;
    krb5_audit_open_fn      open;
    krb5_audit_close_fn     close;
    krb5_audit_kdc_start_fn kdc_start;
    krb5_audit_kdc_stop_fn  kdc_stop;
    krb5_audit_as_req_fn    as_req;
    krb5_audit_tgs_req_fn   tgs_req;
    krb5_audit_s4u2self_fn  tgs_s4u2self;
    krb5_audit_s4u2proxy_fn tgs_s4u2proxy;
    krb5_audit_u2u_fn       tgs_u2u;
} *krb5_audit_vtable;

#endif /* KRB5_AU_PLUGIN_H_INCLUDED */
