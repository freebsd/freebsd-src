/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/rd_safe.c - definition of krb5_rd_safe() */
/*
 * Copyright 1990,1991,2007,2008 by the Massachusetts Institute of Technology.
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
#include "cleanup.h"
#include "auth_con.h"

/*
  parses a KRB_SAFE message from inbuf, placing the integrity-protected user
  data in *outbuf.

  key specifies the key to be used for decryption of the message.

  outbuf points to allocated storage which the caller should free when finished.

  returns system errors, integrity errors
*/
static krb5_error_code
rd_safe_basic(krb5_context context, krb5_auth_context ac,
              const krb5_data *inbuf, krb5_key key,
              krb5_replay_data *replaydata, krb5_data *outbuf)
{
    krb5_error_code       retval;
    krb5_safe           * message;
    krb5_data *safe_body = NULL;
    krb5_checksum our_cksum, *his_cksum;
    krb5_octet zero_octet = 0;
    krb5_data *scratch;
    krb5_boolean valid;
    struct krb5_safe_with_body swb;

    if (!krb5_is_krb_safe(inbuf))
        return KRB5KRB_AP_ERR_MSG_TYPE;

    if ((retval = decode_krb5_safe_with_body(inbuf, &message, &safe_body)))
        return retval;

    if (!krb5_c_valid_cksumtype(message->checksum->checksum_type)) {
        retval = KRB5_PROG_SUMTYPE_NOSUPP;
        goto cleanup;
    }
    if (!krb5_c_is_coll_proof_cksum(message->checksum->checksum_type) ||
        !krb5_c_is_keyed_cksum(message->checksum->checksum_type)) {
        retval = KRB5KRB_AP_ERR_INAPP_CKSUM;
        goto cleanup;
    }

    retval = k5_privsafe_check_addrs(context, ac, message->s_address,
                                     message->r_address);
    if (retval)
        goto cleanup;

    /* verify the checksum */
    /*
     * In order to recreate what was checksummed, we regenerate the message
     * without checksum and then have the cryptographic subsystem verify
     * the checksum for us.  This is because some checksum methods have
     * a confounder encrypted as part of the checksum.
     */
    his_cksum = message->checksum;

    our_cksum.length = 0;
    our_cksum.checksum_type = 0;
    our_cksum.contents = &zero_octet;

    message->checksum = &our_cksum;

    swb.body = safe_body;
    swb.safe = message;
    retval = encode_krb5_safe_with_body(&swb, &scratch);
    message->checksum = his_cksum;
    if (retval)
        goto cleanup;

    retval = krb5_k_verify_checksum(context, key,
                                    KRB5_KEYUSAGE_KRB_SAFE_CKSUM,
                                    scratch, his_cksum, &valid);

    (void) memset(scratch->data, 0, scratch->length);
    krb5_free_data(context, scratch);

    if (!valid) {
        /*
         * Checksum over only the KRB-SAFE-BODY, like RFC 1510 says, in
         * case someone actually implements it correctly.
         */
        retval = krb5_k_verify_checksum(context, key,
                                        KRB5_KEYUSAGE_KRB_SAFE_CKSUM,
                                        safe_body, his_cksum, &valid);
        if (!valid) {
            retval = KRB5KRB_AP_ERR_MODIFIED;
            goto cleanup;
        }
    }

    replaydata->timestamp = message->timestamp;
    replaydata->usec = message->usec;
    replaydata->seq = message->seq_number;

    *outbuf = message->user_data;
    message->user_data.data = NULL;
    retval = 0;

cleanup:
    krb5_free_safe(context, message);
    krb5_free_data(context, safe_body);
    return retval;
}

krb5_error_code KRB5_CALLCONV
krb5_rd_safe(krb5_context context, krb5_auth_context auth_context,
             const krb5_data *inbuf, krb5_data *outbuf,
             krb5_replay_data *outdata)
{
    krb5_error_code       retval;
    krb5_key              key;
    krb5_replay_data      replaydata;

    if (((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_TIME) ||
         (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) &&
        (outdata == NULL))
        /* Need a better error */
        return KRB5_RC_REQUIRED;

    if ((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_TIME) &&
        (auth_context->remote_addr == NULL))
        return KRB5_REMOTE_ADDR_REQUIRED;

    if ((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_TIME) &&
        (auth_context->rcache == NULL))
        return KRB5_RC_REQUIRED;

    /* Get key */
    if ((key = auth_context->recv_subkey) == NULL)
        key = auth_context->key;

    memset(&replaydata, 0, sizeof(replaydata));
    retval = rd_safe_basic(context, auth_context, inbuf, key, &replaydata,
                           outbuf);
    if (retval)
        return retval;

    if (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_TIME) {
        krb5_donot_replay replay;

        if ((retval = krb5_check_clockskew(context, replaydata.timestamp)))
            goto error;

        if ((retval = krb5_gen_replay_name(context, auth_context->remote_addr,
                                           "_safe", &replay.client)))
            goto error;

        replay.server = "";             /* XXX */
        replay.msghash = NULL;
        replay.cusec = replaydata.usec;
        replay.ctime = replaydata.timestamp;
        if ((retval = krb5_rc_store(context, auth_context->rcache, &replay))) {
            free(replay.client);
            goto error;
        }
        free(replay.client);
    }

    if (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) {
        if (!k5_privsafe_check_seqnum(context, auth_context, replaydata.seq)) {
            retval =  KRB5KRB_AP_ERR_BADORDER;
            goto error;
        }
        auth_context->remote_seq_number++;
    }

    if ((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_TIME) ||
        (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) {
        outdata->timestamp = replaydata.timestamp;
        outdata->usec = replaydata.usec;
        outdata->seq = replaydata.seq;
    }

    /* everything is ok - return data to the user */
    return 0;

error:
    free(outbuf->data);
    return retval;

}
