/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/kdc_util.h */
/*
 * Portions Copyright (C) 2007 Apple Inc.
 * Copyright 1990, 2007, 2014 by the Massachusetts Institute of Technology.
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
 * Declarations for policy.c
 */

#ifndef __KRB5_KDC_UTIL__
#define __KRB5_KDC_UTIL__

#include <krb5/kdcpreauth_plugin.h>
#include "kdb.h"
#include "net-server.h"
#include "realm_data.h"
#include "reqstate.h"

krb5_error_code check_hot_list (krb5_ticket *);
krb5_boolean is_local_principal(kdc_realm_t *kdc_active_realm,
                                krb5_const_principal princ1);
krb5_boolean krb5_is_tgs_principal (krb5_const_principal);
krb5_boolean is_cross_tgs_principal(krb5_const_principal);
krb5_error_code
add_to_transited (krb5_data *,
                  krb5_data *,
                  krb5_principal,
                  krb5_principal,
                  krb5_principal);
krb5_error_code
compress_transited (krb5_data *,
                    krb5_principal,
                    krb5_data *);
krb5_error_code
concat_authorization_data (krb5_context,
                           krb5_authdata **,
                           krb5_authdata **,
                           krb5_authdata ***);
krb5_error_code
fetch_last_req_info (krb5_db_entry *, krb5_last_req_entry ***);

krb5_error_code
kdc_convert_key (krb5_keyblock *, krb5_keyblock *, int);
krb5_error_code
kdc_process_tgs_req (kdc_realm_t *, krb5_kdc_req *,
                     const krb5_fulladdr *,
                     krb5_data *,
                     krb5_ticket **,
                     krb5_db_entry **krbtgt_ptr,
                     krb5_keyblock **, krb5_keyblock **,
                     krb5_pa_data **pa_tgs_req);

krb5_error_code
kdc_get_server_key (krb5_context, krb5_ticket *, unsigned int,
                    krb5_boolean match_enctype,
                    krb5_db_entry **, krb5_keyblock **, krb5_kvno *);

krb5_error_code
get_local_tgt(krb5_context context, const krb5_data *realm,
              krb5_db_entry *candidate, krb5_db_entry **alias_out,
              krb5_db_entry **storage_out);

int
validate_as_request (kdc_realm_t *, krb5_kdc_req *, krb5_db_entry,
                     krb5_db_entry, krb5_timestamp,
                     const char **, krb5_pa_data ***);

int
validate_forwardable(krb5_kdc_req *, krb5_db_entry,
                     krb5_db_entry, krb5_timestamp,
                     const char **);

int
validate_tgs_request (kdc_realm_t *, krb5_kdc_req *, krb5_db_entry,
                      krb5_ticket *, krb5_timestamp,
                      const char **, krb5_pa_data ***);

krb5_error_code
check_indicators(krb5_context context, krb5_db_entry *server,
                 krb5_data *const *indicators);

int
fetch_asn1_field (unsigned char *, unsigned int, unsigned int, krb5_data *);

krb5_enctype
select_session_keytype (kdc_realm_t *kdc_active_realm,
                        krb5_db_entry *server,
                        int nktypes,
                        krb5_enctype *ktypes);

void limit_string (char *name);

void
ktypes2str(char *s, size_t len, int nktypes, krb5_enctype *ktype);

void
rep_etypes2str(char *s, size_t len, krb5_kdc_rep *rep);

/* authind.c */
krb5_boolean
authind_contains(krb5_data *const *indicators, const char *ind);

krb5_error_code
authind_add(krb5_context context, const char *ind, krb5_data ***indicators);

krb5_error_code
authind_extract(krb5_context context, krb5_authdata **authdata,
                krb5_data ***indicators);

/* cammac.c */
krb5_error_code
cammac_create(krb5_context context, krb5_enc_tkt_part *enc_tkt_reply,
              krb5_keyblock *server_key, krb5_db_entry *krbtgt,
              krb5_authdata **contents, krb5_authdata ***cammac_out);

krb5_boolean
cammac_check_kdcver(krb5_context context, krb5_cammac *cammac,
                    krb5_enc_tkt_part *enc_tkt, krb5_db_entry *krbtgt);

/* do_as_req.c */
void
process_as_req (krb5_kdc_req *, krb5_data *,
                const krb5_fulladdr *, kdc_realm_t *,
                verto_ctx *, loop_respond_fn, void *);

/* do_tgs_req.c */
krb5_error_code
process_tgs_req (struct server_handle *, krb5_data *,
                 const krb5_fulladdr *,
                 krb5_data ** );
/* dispatch.c */
void
dispatch (void *,
          struct sockaddr *,
          const krb5_fulladdr *,
          krb5_data *,
          int,
          verto_ctx *,
          loop_respond_fn,
          void *);

void
kdc_err(krb5_context call_context, errcode_t code, const char *fmt, ...)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 3, 4)))
#endif
    ;

/* policy.c */
int
against_local_policy_as (krb5_kdc_req *, krb5_db_entry,
                         krb5_db_entry, krb5_timestamp,
                         const char **, krb5_pa_data ***);

int
against_local_policy_tgs (krb5_kdc_req *, krb5_db_entry,
                          krb5_ticket *, const char **,
                          krb5_pa_data ***);

/* kdc_preauth.c */
krb5_boolean
enctype_requires_etype_info_2(krb5_enctype enctype);

const char *
missing_required_preauth (krb5_db_entry *client,
                          krb5_db_entry *server,
                          krb5_enc_tkt_part *enc_tkt_reply);
typedef void (*kdc_hint_respond_fn)(void *arg);
void
get_preauth_hint_list(krb5_kdc_req *request,
                      krb5_kdcpreauth_rock rock, krb5_pa_data ***e_data_out,
                      kdc_hint_respond_fn respond, void *arg);
void
load_preauth_plugins(struct server_handle * handle, krb5_context context,
                     verto_ctx *ctx);
void
unload_preauth_plugins(krb5_context context);

typedef void (*kdc_preauth_respond_fn)(void *arg, krb5_error_code code);

void
check_padata(krb5_context context, krb5_kdcpreauth_rock rock,
             krb5_data *req_pkt, krb5_kdc_req *request,
             krb5_enc_tkt_part *enc_tkt_reply, void **padata_context,
             krb5_pa_data ***e_data, krb5_boolean *typed_e_data,
             kdc_preauth_respond_fn respond, void *state);

krb5_error_code
return_padata(krb5_context context, krb5_kdcpreauth_rock rock,
              krb5_data *req_pkt, krb5_kdc_req *request, krb5_kdc_rep *reply,
              krb5_keyblock *encrypting_key, void **padata_context);

void
free_padata_context(krb5_context context, void *padata_context);

krb5_error_code
add_pa_data_element (krb5_context context,
                     krb5_pa_data *padata,
                     krb5_pa_data ***out_padata,
                     krb5_boolean copy);

/* kdc_preauth_ec.c */
krb5_error_code
kdcpreauth_encrypted_challenge_initvt(krb5_context context, int maj_ver,
                                      int min_ver, krb5_plugin_vtable vtable);

/* kdc_preauth_enctsc.c */
krb5_error_code
kdcpreauth_encrypted_timestamp_initvt(krb5_context context, int maj_ver,
                                      int min_ver, krb5_plugin_vtable vtable);

/* kdc_authdata.c */
krb5_error_code
load_authdata_plugins(krb5_context context);
krb5_error_code
unload_authdata_plugins(krb5_context context);

krb5_error_code
get_auth_indicators(krb5_context context, krb5_enc_tkt_part *enc_tkt,
                    krb5_db_entry *local_tgt, krb5_data ***indicators_out);

krb5_error_code
handle_authdata (krb5_context context,
                 unsigned int flags,
                 krb5_db_entry *client,
                 krb5_db_entry *server,
                 krb5_db_entry *header_server,
                 krb5_db_entry *local_tgt,
                 krb5_keyblock *client_key,
                 krb5_keyblock *server_key,
                 krb5_keyblock *header_key,
                 krb5_data *req_pkt,
                 krb5_kdc_req *request,
                 krb5_const_principal for_user_princ,
                 krb5_enc_tkt_part *enc_tkt_request,
                 krb5_data *const *auth_indicators,
                 krb5_enc_tkt_part *enc_tkt_reply);

/* replay.c */
krb5_error_code kdc_init_lookaside(krb5_context context);
krb5_boolean kdc_check_lookaside (krb5_context, krb5_data *, krb5_data **);
void kdc_insert_lookaside (krb5_context, krb5_data *, krb5_data *);
void kdc_remove_lookaside (krb5_context kcontext, krb5_data *);
void kdc_free_lookaside(krb5_context);

/* kdc_util.c */
void reset_for_hangup(void *);

krb5_boolean
include_pac_p(krb5_context context, krb5_kdc_req *request);

krb5_error_code
return_enc_padata(krb5_context context,
                  krb5_data *req_pkt, krb5_kdc_req *request,
                  krb5_keyblock *reply_key,
                  krb5_db_entry *server,
                  krb5_enc_kdc_rep_part *reply_encpart,
                  krb5_boolean is_referral);

krb5_error_code
kdc_process_s4u2self_req (kdc_realm_t *kdc_active_realm,
                          krb5_kdc_req *request,
                          krb5_const_principal client_princ,
                          const krb5_db_entry *server,
                          krb5_keyblock *tgs_subkey,
                          krb5_keyblock *tgs_session,
                          krb5_timestamp kdc_time,
                          krb5_pa_s4u_x509_user **s4u2self_req,
                          krb5_db_entry **princ_ptr,
                          const char **status);

krb5_error_code
kdc_make_s4u2self_rep (krb5_context context,
                       krb5_keyblock *tgs_subkey,
                       krb5_keyblock *tgs_session,
                       krb5_pa_s4u_x509_user *req_s4u_user,
                       krb5_kdc_rep *reply,
                       krb5_enc_kdc_rep_part *reply_encpart);

krb5_error_code
kdc_process_s4u2proxy_req (kdc_realm_t *kdc_active_realm,
                           krb5_kdc_req *request,
                           const krb5_enc_tkt_part *t2enc,
                           const krb5_db_entry *server,
                           krb5_const_principal server_princ,
                           krb5_const_principal proxy_princ,
                           const char **status);

krb5_error_code
kdc_check_transited_list (kdc_realm_t *kdc_active_realm,
                          const krb5_data *trans,
                          const krb5_data *realm1,
                          const krb5_data *realm2);

krb5_error_code
audit_as_request (krb5_kdc_req *request,
                  krb5_db_entry *client,
                  krb5_db_entry *server,
                  krb5_timestamp authtime,
                  krb5_error_code errcode);

krb5_error_code
audit_tgs_request (krb5_kdc_req *request,
                   krb5_const_principal client,
                   krb5_db_entry *server,
                   krb5_timestamp authtime,
                   krb5_error_code errcode);

krb5_error_code
validate_transit_path(krb5_context context,
                      krb5_const_principal client,
                      krb5_db_entry *server,
                      krb5_db_entry *krbtgt);
void
kdc_get_ticket_endtime(kdc_realm_t *kdc_active_realm,
                       krb5_timestamp now,
                       krb5_timestamp endtime,
                       krb5_timestamp till,
                       krb5_db_entry *client,
                       krb5_db_entry *server,
                       krb5_timestamp *out_endtime);

void
kdc_get_ticket_renewtime(kdc_realm_t *realm, krb5_kdc_req *request,
                         krb5_enc_tkt_part *tgt, krb5_db_entry *client,
                         krb5_db_entry *server, krb5_enc_tkt_part *tkt);

void
log_as_req(krb5_context context, const krb5_fulladdr *from,
           krb5_kdc_req *request, krb5_kdc_rep *reply,
           krb5_db_entry *client, const char *cname,
           krb5_db_entry *server, const char *sname,
           krb5_timestamp authtime,
           const char *status, krb5_error_code errcode, const char *emsg);
void
log_tgs_req(krb5_context ctx, const krb5_fulladdr *from,
            krb5_kdc_req *request, krb5_kdc_rep *reply,
            krb5_principal cprinc, krb5_principal sprinc,
            krb5_principal altcprinc,
            krb5_timestamp authtime,
            unsigned int c_flags,
            const char *status, krb5_error_code errcode, const char *emsg);
void
log_tgs_badtrans(krb5_context ctx, krb5_principal cprinc,
                 krb5_principal sprinc, krb5_data *trcont,
                 krb5_error_code errcode);

void
log_tgs_alt_tgt(krb5_context context, krb5_principal p);

/* FAST*/
enum krb5_fast_kdc_flags {
    KRB5_FAST_REPLY_KEY_USED = 0x1,
    KRB5_FAST_REPLY_KEY_REPLACED = 0x02
};

/*
 * If *requestptr contains FX_FAST padata, compute the armor key, verify the
 * checksum over checksummed_data, decode the FAST request, and substitute
 * *requestptr with the inner request.  Set the armor_key, cookie, and
 * fast_options fields in state.  state->cookie will be set for a non-FAST
 * request if it contains FX_COOKIE padata.  If inner_body_out is non-NULL, set
 * *inner_body_out to a copy of the encoded inner body, or to NULL if the
 * request is not a FAST request.
 */
krb5_error_code
kdc_find_fast (krb5_kdc_req **requestptr,  krb5_data *checksummed_data,
               krb5_keyblock *tgs_subkey, krb5_keyblock *tgs_session,
               struct kdc_request_state *state, krb5_data **inner_body_out);

krb5_error_code
kdc_fast_response_handle_padata (struct kdc_request_state *state,
                                 krb5_kdc_req *request,
                                 krb5_kdc_rep *rep,
                                 krb5_enctype enctype);
krb5_error_code
kdc_fast_handle_error (krb5_context context,
                       struct kdc_request_state *state,
                       krb5_kdc_req *request,
                       krb5_pa_data  **in_padata, krb5_error *err,
                       krb5_data **fast_edata_out);

krb5_error_code kdc_fast_handle_reply_key(struct kdc_request_state *state,
                                          krb5_keyblock *existing_key,
                                          krb5_keyblock **out_key);


krb5_boolean
kdc_fast_hide_client(struct kdc_request_state *state);

krb5_error_code
kdc_handle_protected_negotiation( krb5_context context,
                                  krb5_data *req_pkt, krb5_kdc_req *request,
                                  const krb5_keyblock *reply_key,
                                  krb5_pa_data ***out_enc_padata);

krb5_error_code
kdc_fast_read_cookie(krb5_context context, struct kdc_request_state *state,
                     krb5_kdc_req *req, krb5_db_entry *local_tgt);

krb5_boolean kdc_fast_search_cookie(struct kdc_request_state *state,
                                    krb5_preauthtype pa_type, krb5_data *out);

krb5_error_code kdc_fast_set_cookie(struct kdc_request_state *state,
                                    krb5_preauthtype pa_type,
                                    const krb5_data *data);

krb5_error_code
kdc_fast_make_cookie(krb5_context context, struct kdc_request_state *state,
                     krb5_db_entry *local_tgt,
                     krb5_const_principal client_princ,
                     krb5_pa_data **cookie_out);

/* Information handle for kdcpreauth callbacks.  All pointers are aliases. */
struct krb5_kdcpreauth_rock_st {
    krb5_kdc_req *request;
    krb5_data *inner_body;
    krb5_db_entry *client;
    krb5_key_data *client_key;
    krb5_keyblock *client_keyblock;
    struct kdc_request_state *rstate;
    verto_ctx *vctx;
    krb5_data ***auth_indicators;
};

#define isflagset(flagfield, flag) (flagfield & (flag))
#define setflag(flagfield, flag) (flagfield |= (flag))
#define clear(flagfield, flag) (flagfield &= ~(flag))

#ifndef min
#define min(a, b)       ((a) < (b) ? (a) : (b))
#define max(a, b)       ((a) > (b) ? (a) : (b))
#endif

#define ADDRTYPE2FAMILY(X)                                              \
    ((X) == ADDRTYPE_INET6 ? AF_INET6 : (X) == ADDRTYPE_INET ? AF_INET : -1)

/* RFC 4120: KRB5KDC_ERR_KEY_TOO_WEAK
 * RFC 4556: KRB5KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED */
#define KRB5KDC_ERR_KEY_TOO_WEAK KRB5KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED
/* TGS-REQ options where the service can be a non-TGS principal  */

#define NON_TGT_OPTION (KDC_OPT_FORWARDED | KDC_OPT_PROXY | KDC_OPT_RENEW | \
                        KDC_OPT_VALIDATE)

/* TGS-REQ options which are not compatible with referrals */
#define NO_REFERRAL_OPTION (NON_TGT_OPTION | KDC_OPT_ENC_TKT_IN_SKEY)

/*
 * Mask of KDC options that request the corresponding ticket flag with
 * the same number.  Some of these are invalid for AS-REQs, but
 * validate_as_request() takes care of that.  KDC_OPT_RENEWABLE isn't
 * here because it needs special handling in
 * kdc_get_ticket_renewtime().
 *
 * According to RFC 4120 section 3.1.3 the following AS-REQ options
 * request their corresponding ticket flags if local policy allows:
 *
 * KDC_OPT_FORWARDABLE  KDC_OPT_ALLOW_POSTDATE
 * KDC_OPT_POSTDATED    KDC_OPT_PROXIABLE
 * KDC_OPT_RENEWABLE
 *
 * RFC 1510 section A.6 shows pseudocode indicating that the following
 * TGS-REQ options request their corresponding ticket flags if local
 * policy allows:
 *
 * KDC_OPT_FORWARDABLE  KDC_OPT_FORWARDED
 * KDC_OPT_PROXIABLE    KDC_OPT_PROXY
 * KDC_OPT_POSTDATED    KDC_OPT_RENEWABLE
 *
 * The above list omits KDC_OPT_ALLOW_POSTDATE, but RFC 4120 section
 * 5.4.1 says the TGS also handles it.
 *
 * RFC 6112 makes KDC_OPT_REQUEST_ANONYMOUS the same bit number as
 * TKT_FLG_ANONYMOUS.
 */
#define OPTS_COMMON_FLAGS_MASK                                  \
    (KDC_OPT_FORWARDABLE        | KDC_OPT_FORWARDED     |       \
     KDC_OPT_PROXIABLE          | KDC_OPT_PROXY         |       \
     KDC_OPT_ALLOW_POSTDATE     | KDC_OPT_POSTDATED     |       \
     KDC_OPT_REQUEST_ANONYMOUS)

/* Copy KDC options that request the corresponding ticket flags. */
#define OPTS2FLAGS(x) (x & OPTS_COMMON_FLAGS_MASK)

/*
 * Mask of ticket flags for the TGS to propagate from a ticket to a
 * derivative ticket.
 *
 * RFC 4120 section 2.1 says the following flags are carried forward
 * from an initial ticket to derivative tickets:
 *
 * TKT_FLG_PRE_AUTH
 * TKT_FLG_HW_AUTH
 *
 * RFC 4120 section 2.6 says TKT_FLG_FORWARDED is carried forward to
 * derivative tickets.  Proxy tickets are basically identical to
 * forwarded tickets except that a TGT may never be proxied, therefore
 * tickets derived from proxy tickets should have TKT_FLAG_PROXY set.
 * RFC 4120 and RFC 1510 apparently have an accidental omission in not
 * requiring that tickets derived from a proxy ticket have
 * TKT_FLG_PROXY set.  Previous code also omitted this behavior.
 *
 * RFC 6112 section 4.2 implies that TKT_FLG_ANONYMOUS must be
 * propagated from an anonymous ticket to derivative tickets.
 */
#define TGS_COPIED_FLAGS_MASK                           \
    (TKT_FLG_FORWARDED  | TKT_FLG_PROXY         |       \
     TKT_FLG_PRE_AUTH   | TKT_FLG_HW_AUTH       |       \
     TKT_FLG_ANONYMOUS)

/* Copy appropriate header ticket flags to new ticket. */
#define COPY_TKT_FLAGS(x) (x & TGS_COPIED_FLAGS_MASK)

int check_anon(kdc_realm_t *kdc_active_realm,
               krb5_principal client, krb5_principal server);
int errcode_to_protocol(krb5_error_code code);

char *data2string(krb5_data *d);

#endif /* __KRB5_KDC_UTIL__ */
