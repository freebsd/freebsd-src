/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/mk_priv.c */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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
#include "cleanup.h"
#include "auth_con.h"

static krb5_error_code
mk_priv_basic(krb5_context context, const krb5_data *userdata,
              krb5_key key, krb5_replay_data *replaydata,
              krb5_address *local_addr, krb5_address *remote_addr,
              krb5_data *cstate, krb5_data *outbuf)
{
    krb5_enctype        enctype = krb5_k_key_enctype(context, key);
    krb5_error_code     retval;
    krb5_priv           privmsg;
    krb5_priv_enc_part  privmsg_enc_part;
    krb5_data           *scratch1, *scratch2;
    size_t              enclen;

    privmsg.enc_part.kvno = 0;  /* XXX allow user-set? */
    privmsg.enc_part.enctype = enctype;

    privmsg_enc_part.user_data = *userdata;
    privmsg_enc_part.s_address = local_addr;
    privmsg_enc_part.r_address = remote_addr;

    /* We should check too make sure one exists. */
    privmsg_enc_part.timestamp  = replaydata->timestamp;
    privmsg_enc_part.usec       = replaydata->usec;
    privmsg_enc_part.seq_number = replaydata->seq;

    /* start by encoding to-be-encrypted part of the message */
    if ((retval = encode_krb5_enc_priv_part(&privmsg_enc_part, &scratch1)))
        return retval;

    /* put together an eblock for this encryption */
    if ((retval = krb5_c_encrypt_length(context, enctype,
                                        scratch1->length, &enclen)))
        goto clean_scratch;

    privmsg.enc_part.ciphertext.length = enclen;
    if (!(privmsg.enc_part.ciphertext.data =
          malloc(privmsg.enc_part.ciphertext.length))) {
        retval = ENOMEM;
        goto clean_scratch;
    }

    if ((retval = krb5_k_encrypt(context, key,
                                 KRB5_KEYUSAGE_KRB_PRIV_ENCPART,
                                 (cstate->length > 0) ? cstate : NULL,
                                 scratch1, &privmsg.enc_part)))
        goto clean_encpart;

    if ((retval = encode_krb5_priv(&privmsg, &scratch2)))
        goto clean_encpart;

    *outbuf = *scratch2;
    free(scratch2);
    retval = 0;

clean_encpart:
    memset(privmsg.enc_part.ciphertext.data, 0,
           privmsg.enc_part.ciphertext.length);
    free(privmsg.enc_part.ciphertext.data);
    privmsg.enc_part.ciphertext.length = 0;
    privmsg.enc_part.ciphertext.data = 0;

clean_scratch:
    memset(scratch1->data, 0, scratch1->length);
    krb5_free_data(context, scratch1);

    return retval;
}


krb5_error_code KRB5_CALLCONV
krb5_mk_priv(krb5_context context, krb5_auth_context auth_context,
             const krb5_data *userdata, krb5_data *outbuf,
             krb5_replay_data *outdata)
{
    krb5_error_code       retval;
    krb5_key              key;
    krb5_replay_data      replaydata;
    krb5_data             buf = empty_data();

    *outbuf = empty_data();

    /* Clear replaydata block */
    memset(&replaydata, 0, sizeof(krb5_replay_data));

    /* Get keyblock */
    if ((key = auth_context->send_subkey) == NULL)
        key = auth_context->key;

    /* Get replay info */
    if ((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_TIME) &&
        (auth_context->rcache == NULL))
        return KRB5_RC_REQUIRED;

    if (((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_TIME) ||
         (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) &&
        (outdata == NULL))
        /* Need a better error */
        return KRB5_RC_REQUIRED;

    if (!auth_context->local_addr)
        return KRB5_LOCAL_ADDR_REQUIRED;

    if ((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_TIME) ||
        (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_TIME)) {
        if ((retval = krb5_us_timeofday(context, &replaydata.timestamp,
                                        &replaydata.usec)))
            return retval;
        if (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_TIME) {
            outdata->timestamp = replaydata.timestamp;
            outdata->usec = replaydata.usec;
        }
    }
    if ((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) ||
        (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) {
        replaydata.seq = auth_context->local_seq_number++;
        if (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)
            outdata->seq = replaydata.seq;
    }

    {
        krb5_address * premote_fulladdr = NULL;
        krb5_address * plocal_fulladdr;
        krb5_address remote_fulladdr;
        krb5_address local_fulladdr;
        CLEANUP_INIT(2);

        if (auth_context->local_port) {
            if (!(retval = krb5_make_fulladdr(context, auth_context->local_addr,
                                              auth_context->local_port,
                                              &local_fulladdr))) {
                CLEANUP_PUSH(local_fulladdr.contents, free);
                plocal_fulladdr = &local_fulladdr;
            } else {
                goto error;
            }
        } else {
            plocal_fulladdr = auth_context->local_addr;
        }

        if (auth_context->remote_addr) {
            if (auth_context->remote_port) {
                if (!(retval = krb5_make_fulladdr(context,auth_context->remote_addr,
                                                  auth_context->remote_port,
                                                  &remote_fulladdr))){
                    CLEANUP_PUSH(remote_fulladdr.contents, free);
                    premote_fulladdr = &remote_fulladdr;
                } else {
                    CLEANUP_DONE();
                    goto error;
                }
            } else {
                premote_fulladdr = auth_context->remote_addr;
            }
        }

        if ((retval = mk_priv_basic(context, userdata, key, &replaydata,
                                    plocal_fulladdr, premote_fulladdr,
                                    &auth_context->cstate, &buf))) {
            CLEANUP_DONE();
            goto error;
        }

        CLEANUP_DONE();
    }

    if (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_TIME) {
        krb5_donot_replay replay;

        if ((retval = krb5_gen_replay_name(context, auth_context->local_addr,
                                           "_priv", &replay.client)))
            goto error;

        replay.server = "";             /* XXX */
        replay.msghash = NULL;
        replay.cusec = replaydata.usec;
        replay.ctime = replaydata.timestamp;
        if ((retval = krb5_rc_store(context, auth_context->rcache, &replay))) {
            /* should we really error out here? XXX */
            free(replay.client);
            goto error;
        }
        free(replay.client);
    }

    *outbuf = buf;
    return 0;

error:
    krb5_free_data_contents(context, &buf);
    if ((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) ||
        (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE))
        auth_context->local_seq_number--;

    return retval;
}
