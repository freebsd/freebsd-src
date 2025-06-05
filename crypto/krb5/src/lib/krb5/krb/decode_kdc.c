/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/decode_kdc.c */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "int-proto.h"
#include "fast.h"

/*
  Takes a KDC_REP message and decrypts encrypted part using etype and
  *key, putting result in *rep.
  dec_rep->client,ticket,session,last_req,server,caddrs
  are all set to allocated storage which should be freed by the caller
  when finished with the response.

  If the response isn't a KDC_REP (tgs or as), it returns an error from
  the decoding routines.

  returns errors from encryption routines, system errors
*/

krb5_error_code
krb5int_decode_tgs_rep(krb5_context context,
                       struct krb5int_fast_request_state *fast_state,
                       krb5_data *enc_rep, const krb5_keyblock *key,
                       krb5_keyusage usage, krb5_kdc_rep **dec_rep_out)
{
    krb5_error_code retval;
    krb5_kdc_rep *dec_rep = NULL;
    krb5_keyblock *strengthen_key = NULL, tgs_key;

    tgs_key.contents = NULL;
    if (krb5_is_as_rep(enc_rep))
        retval = decode_krb5_as_rep(enc_rep, &dec_rep);
    else if (krb5_is_tgs_rep(enc_rep))
        retval = decode_krb5_tgs_rep(enc_rep, &dec_rep);
    else
        retval = KRB5KRB_AP_ERR_MSG_TYPE;
    if (retval)
        goto cleanup;

    retval = krb5int_fast_process_response(context, fast_state, dec_rep,
                                           &strengthen_key);
    if (retval == KRB5_ERR_FAST_REQUIRED)
        retval = 0;
    else if (retval)
        goto cleanup;
    retval = krb5int_fast_reply_key(context, strengthen_key, key, &tgs_key);
    if (retval)
        goto cleanup;

    retval = krb5_kdc_rep_decrypt_proc(context, &tgs_key, &usage, dec_rep);
    if (retval)
        goto cleanup;

    *dec_rep_out = dec_rep;
    dec_rep = NULL;

cleanup:
    krb5_free_kdc_rep(context, dec_rep);
    krb5_free_keyblock(context, strengthen_key);
    krb5_free_keyblock_contents(context, &tgs_key);
    return retval;
}
