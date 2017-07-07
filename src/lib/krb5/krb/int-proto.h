/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/int-proto.h - Prototypes for libkrb5 internal functions */
/*
 * Copyright 1990,1991 the Massachusetts Institute of Technology.
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

#ifndef KRB5_INT_FUNC_PROTO__
#define KRB5_INT_FUNC_PROTO__

struct krb5int_fast_request_state;

typedef struct k5_response_items_st k5_response_items;

typedef krb5_error_code
(*get_as_key_fn)(krb5_context, krb5_principal, krb5_enctype, krb5_prompter_fct,
                 void *prompter_data, krb5_data *salt, krb5_data *s2kparams,
                 krb5_keyblock *as_key, void *gak_data,
                 k5_response_items *ritems);

krb5_error_code
krb5int_tgtname(krb5_context context, const krb5_data *, const krb5_data *,
                krb5_principal *);

krb5_error_code
krb5int_libdefault_boolean(krb5_context, const krb5_data *, const char *,
                           int *);
krb5_error_code
krb5int_libdefault_string(krb5_context context, const krb5_data *realm,
                          const char *option, char **ret_value);


krb5_error_code krb5_ser_authdata_init (krb5_context);
krb5_error_code krb5_ser_address_init (krb5_context);
krb5_error_code krb5_ser_authenticator_init (krb5_context);
krb5_error_code krb5_ser_checksum_init (krb5_context);
krb5_error_code krb5_ser_keyblock_init (krb5_context);
krb5_error_code krb5_ser_principal_init (krb5_context);
krb5_error_code krb5_ser_authdata_context_init (krb5_context);

krb5_error_code
krb5_preauth_supply_preauth_data(krb5_context context,
                                 krb5_get_init_creds_opt *opt,
                                 const char *attr, const char *value);

krb5_error_code
clpreauth_encrypted_challenge_initvt(krb5_context context, int maj_ver,
                                     int min_ver, krb5_plugin_vtable vtable);

krb5_error_code
clpreauth_encrypted_timestamp_initvt(krb5_context context, int maj_ver,
                                     int min_ver, krb5_plugin_vtable vtable);

krb5_error_code
clpreauth_sam2_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable);

krb5_error_code
clpreauth_otp_initvt(krb5_context context, int maj_ver, int min_ver,
                     krb5_plugin_vtable vtable);

krb5_error_code
krb5int_construct_matching_creds(krb5_context context, krb5_flags options,
                                 krb5_creds *in_creds, krb5_creds *mcreds,
                                 krb5_flags *fields);

#define in_clock_skew(date, now) (labs((date)-(now)) < context->clockskew)

#define IS_TGS_PRINC(p) ((p)->length == 2 &&                            \
                         data_eq_string((p)->data[0], KRB5_TGS_NAME))

typedef krb5_error_code
(*k5_pacb_fn)(krb5_context context, krb5_keyblock *subkey, krb5_kdc_req *req,
              void *arg);

krb5_error_code
krb5_get_cred_via_tkt_ext(krb5_context context, krb5_creds *tkt,
                          krb5_flags kdcoptions, krb5_address *const *address,
                          krb5_pa_data **in_padata, krb5_creds *in_cred,
                          k5_pacb_fn pacb_fn, void *pacb_data,
                          krb5_pa_data ***out_padata,
                          krb5_pa_data ***enc_padata, krb5_creds **out_cred,
                          krb5_keyblock **out_subkey);

krb5_error_code
k5_make_tgs_req(krb5_context context, struct krb5int_fast_request_state *,
                krb5_creds *tkt, krb5_flags kdcoptions,
                krb5_address *const *address, krb5_pa_data **in_padata,
                krb5_creds *in_cred, k5_pacb_fn pacb_fn, void *pacb_data,
                krb5_data *req_asn1_out, krb5_timestamp *timestamp_out,
                krb5_int32 *nonce_out, krb5_keyblock **subkey_out);

krb5_error_code
krb5int_process_tgs_reply(krb5_context context,
                          struct krb5int_fast_request_state *,
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
                          krb5_creds **out_cred);

/* The subkey field is an output parameter; if a
 * tgs-rep is received then the subkey will be filled
 * in with the subkey needed to decrypt the TGS
 * response. Otherwise it will be set to null.
 */
krb5_error_code krb5int_decode_tgs_rep(krb5_context,
                                       struct krb5int_fast_request_state *,
                                       krb5_data *,
                                       const krb5_keyblock *, krb5_keyusage,
                                       krb5_kdc_rep ** );

krb5_error_code
krb5int_validate_times(krb5_context, krb5_ticket_times *);

krb5_error_code
krb5int_copy_authdatum(krb5_context, const krb5_authdata *, krb5_authdata **);

krb5_boolean
k5_privsafe_check_seqnum(krb5_context ctx, krb5_auth_context ac,
                         krb5_ui_4 in_seq);

krb5_error_code
k5_privsafe_check_addrs(krb5_context context, krb5_auth_context ac,
                        krb5_address *msg_s_addr, krb5_address *msg_r_addr);

krb5_error_code
krb5int_mk_chpw_req(krb5_context context, krb5_auth_context auth_context,
                    krb5_data *ap_req, const char *passwd, krb5_data *packet);

krb5_error_code
krb5int_rd_chpw_rep(krb5_context context, krb5_auth_context auth_context,
                    krb5_data *packet, int *result_code,
                    krb5_data *result_data);

krb5_error_code KRB5_CALLCONV
krb5_chpw_result_code_string(krb5_context context, int result_code,
                             char **result_codestr);

krb5_error_code
krb5int_mk_setpw_req(krb5_context context, krb5_auth_context auth_context,
                     krb5_data *ap_req, krb5_principal targetprinc,
                     const char *passwd, krb5_data *packet);

void
k5_ccselect_free_context(krb5_context context);

krb5_error_code
k5_init_creds_get(krb5_context context, krb5_init_creds_context ctx,
                  int *use_master);

krb5_error_code
k5_init_creds_current_time(krb5_context context, krb5_init_creds_context ctx,
                           krb5_boolean allow_unauth, krb5_timestamp *time_out,
                           krb5_int32 *usec_out);

krb5_error_code
k5_preauth(krb5_context context, krb5_init_creds_context ctx,
           krb5_pa_data **in_padata, krb5_boolean must_preauth,
           krb5_pa_data ***padata_out, krb5_preauthtype *pa_type_out);

krb5_error_code
k5_preauth_tryagain(krb5_context context, krb5_init_creds_context ctx,
                    krb5_pa_data **in_padata, krb5_pa_data ***padata_out);

void
k5_init_preauth_context(krb5_context context);

void
k5_free_preauth_context(krb5_context context);

void
k5_reset_preauth_types_tried(krb5_context context);

void
k5_preauth_prepare_request(krb5_context context, krb5_get_init_creds_opt *opt,
                           krb5_kdc_req *request);

void
k5_preauth_request_context_init(krb5_context context);

void
k5_preauth_request_context_fini(krb5_context context);

krb5_error_code
k5_response_items_new(k5_response_items **ri_out);

void
k5_response_items_free(k5_response_items *ri);

void
k5_response_items_reset(k5_response_items *ri);

krb5_boolean
k5_response_items_empty(const k5_response_items *ri);

const char * const *
k5_response_items_list_questions(const k5_response_items *ri);

krb5_error_code
k5_response_items_ask_question(k5_response_items *ri, const char *question,
                               const char *challenge);

const char *
k5_response_items_get_challenge(const k5_response_items *ri,
                                const char *question);

krb5_error_code
k5_response_items_set_answer(k5_response_items *ri, const char *question,
                             const char *answer);

const char *
k5_response_items_get_answer(const k5_response_items *ri,
                             const char *question);

/* Save code and its extended message (if any) in out. */
void
k5_save_ctx_error(krb5_context ctx, krb5_error_code code, struct errinfo *out);

/* Return the code from in and restore its extended message (if any). */
krb5_error_code
k5_restore_ctx_error(krb5_context ctx, struct errinfo *in);

krb5_error_code
k5_encrypt_keyhelper(krb5_context context, krb5_key key,
                     krb5_keyusage keyusage, const krb5_data *plain,
                     krb5_enc_data *cipher);

krb5_error_code KRB5_CALLCONV
k5_get_init_creds(krb5_context context, krb5_creds *creds,
                  krb5_principal client, krb5_prompter_fct prompter,
                  void *prompter_data, krb5_deltat start_time,
                  const char *in_tkt_service, krb5_get_init_creds_opt *options,
                  get_as_key_fn gak, void *gak_data, int *master,
                  krb5_kdc_rep **as_reply);

krb5_error_code
k5_populate_gic_opt(krb5_context context, krb5_get_init_creds_opt **opt,
                    krb5_flags options, krb5_address *const *addrs,
                    krb5_enctype *ktypes, krb5_preauthtype *pre_auth_types,
                    krb5_creds *creds);

krb5_error_code
k5_copy_creds_contents(krb5_context, const krb5_creds *, krb5_creds *);

krb5_error_code
k5_build_conf_principals(krb5_context context, krb5_ccache id,
                         krb5_const_principal principal, const char *name,
                         krb5_creds *cred);

krb5_error_code
k5_generate_and_save_subkey(krb5_context context,
                            krb5_auth_context auth_context,
                            krb5_keyblock *keyblock, krb5_enctype enctype);

krb5_error_code
k5_client_realm_path(krb5_context context, const krb5_data *client,
                     const krb5_data *server, krb5_data **rpath_out);

size_t
k5_count_etypes(const krb5_enctype *list);

krb5_error_code
k5_copy_etypes(const krb5_enctype *old_list, krb5_enctype **new_list);

krb5_ccache
k5_gic_opt_get_in_ccache(krb5_get_init_creds_opt *opt);

krb5_ccache
k5_gic_opt_get_out_ccache(krb5_get_init_creds_opt *opt);

const char *
k5_gic_opt_get_fast_ccache_name(krb5_get_init_creds_opt *opt);

krb5_flags
k5_gic_opt_get_fast_flags(krb5_get_init_creds_opt *opt);

void
k5_gic_opt_get_expire_cb(krb5_get_init_creds_opt *opt,
                         krb5_expire_callback_func *cb_out, void **data_out);

void
k5_gic_opt_get_responder(krb5_get_init_creds_opt *opt,
                         krb5_responder_fn *responder_out, void **data_out);

/*
 * Make a shallow copy of opt, with all pointer fields aliased, or NULL on an
 * out-of-memory failure.  The caller must free the result with free, and must
 * not use it with the following functions:
 *
 *     krb5_get_init_creds_opt_free
 *     krb5_get_init_creds_opt_set_pa
 *     krb5_get_init_creds_opt_set_fast_ccache
 *     krb5_get_init_creds_opt_set_fast_ccache_name
 */
krb5_get_init_creds_opt *
k5_gic_opt_shallow_copy(krb5_get_init_creds_opt *opt);

/* Return -1 if no PAC request option was specified, or the option value as a
 * boolean (0 or 1). */
int
k5_gic_opt_pac_request(krb5_get_init_creds_opt *opt);

#endif /* KRB5_INT_FUNC_PROTO__ */
