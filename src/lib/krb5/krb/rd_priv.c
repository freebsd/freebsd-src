/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/rd_priv.c */
/*
 * Copyright 1990,1991,2007 by the Massachusetts Institute of Technology.
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
#include "auth_con.h"

/*

  Parses a KRB_PRIV message from inbuf, placing the confidential user
  data in *outbuf.

  key specifies the key to be used for decryption of the message.

  outbuf points to allocated storage which the caller should
  free when finished.

  Returns system errors, integrity errors.

*/

static krb5_error_code
rd_priv_basic(krb5_context context, krb5_auth_context ac,
              const krb5_data *inbuf, const krb5_key key,
              krb5_replay_data *replaydata, krb5_data *outbuf)
{
    krb5_error_code       retval;
    krb5_priv           * privmsg;
    krb5_data             scratch;
    krb5_priv_enc_part  * privmsg_enc_part;
    krb5_data             *iv = NULL;

    if (!krb5_is_krb_priv(inbuf))
        return KRB5KRB_AP_ERR_MSG_TYPE;

    /* decode private message */
    if ((retval = decode_krb5_priv(inbuf, &privmsg)))
        return retval;

    if (ac->cstate.length > 0)
        iv = &ac->cstate;

    scratch.length = privmsg->enc_part.ciphertext.length;
    if (!(scratch.data = malloc(scratch.length))) {
        retval = ENOMEM;
        goto cleanup_privmsg;
    }

    if ((retval = krb5_k_decrypt(context, key,
                                 KRB5_KEYUSAGE_KRB_PRIV_ENCPART, iv,
                                 &privmsg->enc_part, &scratch)))
        goto cleanup_scratch;

    /*  now decode the decrypted stuff */
    if ((retval = decode_krb5_enc_priv_part(&scratch, &privmsg_enc_part)))
        goto cleanup_scratch;

    retval = k5_privsafe_check_addrs(context, ac, privmsg_enc_part->s_address,
                                     privmsg_enc_part->r_address);
    if (retval)
        goto cleanup_data;

    replaydata->timestamp = privmsg_enc_part->timestamp;
    replaydata->usec = privmsg_enc_part->usec;
    replaydata->seq = privmsg_enc_part->seq_number;

    /* everything is ok - return data to the user */
    *outbuf = privmsg_enc_part->user_data;
    retval = 0;

cleanup_data:;
    if (retval == 0)
        privmsg_enc_part->user_data.data = 0;
    krb5_free_priv_enc_part(context, privmsg_enc_part);

cleanup_scratch:;
    memset(scratch.data, 0, scratch.length);
    free(scratch.data);

cleanup_privmsg:;
    free(privmsg->enc_part.ciphertext.data);
    free(privmsg);

    return retval;
}

krb5_error_code KRB5_CALLCONV
krb5_rd_priv(krb5_context context, krb5_auth_context auth_context,
             const krb5_data *inbuf, krb5_data *outbuf,
             krb5_replay_data *outdata)
{
    krb5_error_code       retval;
    krb5_key              key;
    krb5_replay_data      replaydata;

    /* Get key */
    if ((key = auth_context->recv_subkey) == NULL)
        key = auth_context->key;

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

    memset(&replaydata, 0, sizeof(replaydata));
    retval = rd_priv_basic(context, auth_context, inbuf, key, &replaydata,
                           outbuf);
    if (retval)
        return retval;

    if (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_TIME) {
        krb5_donot_replay replay;

        if ((retval = krb5_check_clockskew(context, replaydata.timestamp)))
            goto error;

        if ((retval = krb5_gen_replay_name(context, auth_context->remote_addr,
                                           "_priv", &replay.client)))
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

error:;
    free(outbuf->data);
    outbuf->length = 0;
    outbuf->data = NULL;

    return retval;
}
