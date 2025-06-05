/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/fast.h */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#ifndef KRB_FAST_H

#define KRB_FAST_H

#include <k5-int.h>

struct krb5int_fast_request_state {
    krb5_kdc_req fast_outer_request;
    krb5_keyblock *armor_key; /*non-null means fast is in use*/
    krb5_fast_armor *armor;
    krb5_ui_4 fast_state_flags;
    krb5_ui_4 fast_options;
    krb5_int32 nonce;
};

#define KRB5INT_FAST_DO_FAST     (1l<<0)  /* Perform FAST */
#define KRB5INT_FAST_ARMOR_AVAIL (1l<<1)

krb5_error_code
krb5int_fast_prep_req_body(krb5_context context,
                           struct krb5int_fast_request_state *state,
                           krb5_kdc_req *request,
                           krb5_data **encoded_req_body);

typedef krb5_error_code (*kdc_req_encoder_proc)(const krb5_kdc_req *,
                                                krb5_data **);

krb5_error_code
krb5int_fast_prep_req(krb5_context context,
                      struct krb5int_fast_request_state *state,
                      krb5_kdc_req *request,
                      const krb5_data *to_be_checksummed,
                      kdc_req_encoder_proc encoder,
                      krb5_data **encoded_request);

krb5_error_code
krb5int_fast_process_error(krb5_context context,
                           struct krb5int_fast_request_state *state,
                           krb5_error **err_replyptr,
                           krb5_pa_data ***out_padata,
                           krb5_boolean *retry);

krb5_error_code
krb5int_fast_process_response(krb5_context context,
                              struct krb5int_fast_request_state *state,
                              krb5_kdc_rep *resp,
                              krb5_keyblock **strengthen_key);

krb5_error_code
krb5int_fast_make_state(krb5_context context,
                        struct krb5int_fast_request_state **state);

void
krb5int_fast_free_state(krb5_context context,
                        struct krb5int_fast_request_state *state);

krb5_error_code
krb5int_fast_as_armor(krb5_context context,
                      struct krb5int_fast_request_state *state,
                      krb5_get_init_creds_opt *opt, krb5_kdc_req *request);

krb5_error_code
krb5int_fast_reply_key(krb5_context context,
                       const krb5_keyblock *strengthen_key,
                       const krb5_keyblock *existing_key, krb5_keyblock *output_key);


krb5_error_code
krb5int_fast_verify_nego(krb5_context context,
                         struct krb5int_fast_request_state *state,
                         krb5_kdc_rep *rep, krb5_data *request,
                         krb5_keyblock *decrypting_key,
                         krb5_boolean *fast_avail);

krb5_boolean
k5_upgrade_to_fast_p(krb5_context context,
                     struct krb5int_fast_request_state *state,
                     krb5_pa_data **padata);

krb5_error_code
krb5int_fast_tgs_armor(krb5_context context,
                       struct krb5int_fast_request_state *state,
                       krb5_keyblock *subkey,
                       krb5_keyblock *session_key,
                       krb5_ccache ccache,
                       krb5_data *target_realm);

#endif
