/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef KRB5_INIT_CREDS_CONTEXT
#define KRB5_INIT_CREDS_CONTEXT 1

#include "k5-json.h"
#include "int-proto.h"

struct krb5_responder_context_st {
    k5_response_items *items;
};

struct gak_password {
    krb5_data storage;
    const krb5_data *password;
};

struct _krb5_init_creds_context {
    krb5_get_init_creds_opt *opt;
    krb5_get_init_creds_opt opt_storage;
    char *in_tkt_service;
    krb5_prompter_fct prompter;
    void *prompter_data;
    get_as_key_fn gak_fct;
    void *gak_data;
    krb5_timestamp request_time;
    krb5_deltat start_time;
    krb5_deltat tkt_life;
    krb5_deltat renew_life;
    krb5_boolean complete;
    unsigned int loopcount;
    struct gak_password gakpw;
    krb5_error *err_reply;
    krb5_pa_data **err_padata;
    krb5_creds cred;
    krb5_kdc_req *request;
    krb5_kdc_rep *reply;
    /**
     * Stores the outer request body in order to feed into FAST for
     * checksumming.  This is maintained even if FAST is not used. This is not
     * used for preauth: that requires the inner request body.  For AS-only
     * FAST it would be better for krb5int_fast_prep_req() to simply generate
     * this.  However for TGS FAST, the client needs to supply the
     * to_be_checksummed data. Whether this should be refactored should be
     * revisited as TGS fast is integrated.
     */
    krb5_data *outer_request_body;
    krb5_data *inner_request_body; /**< For preauth */
    krb5_data *encoded_previous_request;
    struct krb5int_fast_request_state *fast_state;
    krb5_pa_data **preauth_to_use;
    krb5_boolean default_salt;
    krb5_data salt;
    krb5_data s2kparams;
    krb5_keyblock as_key;
    krb5_enctype etype;
    krb5_boolean enc_pa_rep_permitted;
    krb5_boolean restarted;
    krb5_boolean sent_nontrivial_preauth;
    krb5_boolean preauth_required;
    struct krb5_responder_context_st rctx;
    krb5_preauthtype selected_preauth_type;
    krb5_preauthtype allowed_preauth_type;
    k5_json_object cc_config_in;
    k5_json_object cc_config_out;
    /* Discovered offset of server time during preauth */
    krb5_timestamp pa_offset;
    krb5_int32 pa_offset_usec;
    enum { NO_OFFSET = 0, UNAUTH_OFFSET, AUTH_OFFSET } pa_offset_state;
};

krb5_error_code
krb5_get_as_key_password(krb5_context context,
                         krb5_principal client,
                         krb5_enctype etype,
                         krb5_prompter_fct prompter,
                         void *prompter_data,
                         krb5_data *salt,
                         krb5_data *params,
                         krb5_keyblock *as_key,
                         void *gak_data,
                         k5_response_items *ritems);

#endif /* !KRB5_INIT_CREDS_CONTEXT */
