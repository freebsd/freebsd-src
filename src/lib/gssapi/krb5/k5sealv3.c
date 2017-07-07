/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/k5sealv3.c */
/*
 * Copyright 2003,2004,2007 by the Massachusetts Institute of Technology.
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

/* draft-ietf-krb-wg-gssapi-cfx-05 */

#include "k5-int.h"
#include "gssapiP_krb5.h"

int
gss_krb5int_rotate_left (void *ptr, size_t bufsiz, size_t rc)
{
    /* Optimize for receiving.  After some debugging is done, the MIT
       implementation won't do any rotates on sending, and while
       debugging, they'll be randomly chosen.

       Return 1 for success, 0 for failure (ENOMEM).  */
    void *tbuf;

    if (bufsiz == 0)
        return 1;
    rc = rc % bufsiz;
    if (rc == 0)
        return 1;

    tbuf = malloc(rc);
    if (tbuf == 0)
        return 0;
    memcpy(tbuf, ptr, rc);
    memmove(ptr, (char *)ptr + rc, bufsiz - rc);
    memcpy((char *)ptr + bufsiz - rc, tbuf, rc);
    free(tbuf);
    return 1;
}

static const gss_buffer_desc empty_message = { 0, 0 };

krb5_error_code
gss_krb5int_make_seal_token_v3 (krb5_context context,
                                krb5_gss_ctx_id_rec *ctx,
                                const gss_buffer_desc * message,
                                gss_buffer_t token,
                                int conf_req_flag, int toktype)
{
    size_t bufsize = 16;
    unsigned char *outbuf = 0;
    krb5_error_code err;
    int key_usage;
    unsigned char acceptor_flag;
    const gss_buffer_desc *message2 = message;
#ifdef CFX_EXERCISE
    size_t rrc;
#endif
    size_t ec;
    unsigned short tok_id;
    krb5_checksum sum;
    krb5_key key;
    krb5_cksumtype cksumtype;

    acceptor_flag = ctx->initiate ? 0 : FLAG_SENDER_IS_ACCEPTOR;
    key_usage = (toktype == KG_TOK_WRAP_MSG
                 ? (ctx->initiate
                    ? KG_USAGE_INITIATOR_SEAL
                    : KG_USAGE_ACCEPTOR_SEAL)
                 : (ctx->initiate
                    ? KG_USAGE_INITIATOR_SIGN
                    : KG_USAGE_ACCEPTOR_SIGN));
    if (ctx->have_acceptor_subkey) {
        key = ctx->acceptor_subkey;
        cksumtype = ctx->acceptor_subkey_cksumtype;
    } else {
        key = ctx->subkey;
        cksumtype = ctx->cksumtype;
    }
    assert(key != NULL);

#ifdef CFX_EXERCISE
    {
        static int initialized = 0;
        if (!initialized) {
            srand(time(0));
            initialized = 1;
        }
    }
#endif

    if (toktype == KG_TOK_WRAP_MSG && conf_req_flag) {
        krb5_data plain;
        krb5_enc_data cipher;
        size_t ec_max;

        /* 300: Adds some slop.  */
        if (SIZE_MAX - 300 < message->length)
            return ENOMEM;
        ec_max = SIZE_MAX - message->length - 300;
        if (ec_max > 0xffff)
            ec_max = 0xffff;
#ifdef CFX_EXERCISE
        /* For testing only.  For performance, always set ec = 0.  */
        ec = ec_max & rand();
#else
        ec = 0;
#endif
        err = alloc_data(&plain, message->length + 16 + ec);
        if (err)
            return err;

        /* Get size of ciphertext.  */
        bufsize = 16 + krb5_encrypt_size (plain.length, key->keyblock.enctype);
        /* Allocate space for header plus encrypted data.  */
        outbuf = gssalloc_malloc(bufsize);
        if (outbuf == NULL) {
            free(plain.data);
            return ENOMEM;
        }

        /* TOK_ID */
        store_16_be(KG2_TOK_WRAP_MSG, outbuf);
        /* flags */
        outbuf[2] = (acceptor_flag
                     | (conf_req_flag ? FLAG_WRAP_CONFIDENTIAL : 0)
                     | (ctx->have_acceptor_subkey ? FLAG_ACCEPTOR_SUBKEY : 0));
        /* filler */
        outbuf[3] = 0xff;
        /* EC */
        store_16_be(ec, outbuf+4);
        /* RRC */
        store_16_be(0, outbuf+6);
        store_64_be(ctx->seq_send, outbuf+8);

        memcpy(plain.data, message->value, message->length);
        if (ec != 0)
            memset(plain.data + message->length, 'x', ec);
        memcpy(plain.data + message->length + ec, outbuf, 16);

        cipher.ciphertext.data = (char *)outbuf + 16;
        cipher.ciphertext.length = bufsize - 16;
        cipher.enctype = key->keyblock.enctype;
        err = krb5_k_encrypt(context, key, key_usage, 0, &plain, &cipher);
        zap(plain.data, plain.length);
        free(plain.data);
        plain.data = 0;
        if (err)
            goto error;

        /* Now that we know we're returning a valid token....  */
        ctx->seq_send++;

#ifdef CFX_EXERCISE
        rrc = rand() & 0xffff;
        if (gss_krb5int_rotate_left(outbuf+16, bufsize-16,
                                    (bufsize-16) - (rrc % (bufsize - 16))))
            store_16_be(rrc, outbuf+6);
        /* If the rotate fails, don't worry about it.  */
#endif
    } else if (toktype == KG_TOK_WRAP_MSG && !conf_req_flag) {
        krb5_data plain;
        size_t cksumsize;

        /* Here, message is the application-supplied data; message2 is
           what goes into the output token.  They may be the same, or
           message2 may be empty (for MIC).  */

        tok_id = KG2_TOK_WRAP_MSG;

    wrap_with_checksum:
        err = alloc_data(&plain, message->length + 16);
        if (err)
            return err;

        err = krb5_c_checksum_length(context, cksumtype, &cksumsize);
        if (err)
            goto error;

        assert(cksumsize <= 0xffff);

        bufsize = 16 + message2->length + cksumsize;
        outbuf = gssalloc_malloc(bufsize);
        if (outbuf == NULL) {
            free(plain.data);
            plain.data = 0;
            err = ENOMEM;
            goto error;
        }

        /* TOK_ID */
        store_16_be(tok_id, outbuf);
        /* flags */
        outbuf[2] = (acceptor_flag
                     | (ctx->have_acceptor_subkey ? FLAG_ACCEPTOR_SUBKEY : 0));
        /* filler */
        outbuf[3] = 0xff;
        if (toktype == KG_TOK_WRAP_MSG) {
            /* Use 0 for checksum calculation, substitute
               checksum length later.  */
            /* EC */
            store_16_be(0, outbuf+4);
            /* RRC */
            store_16_be(0, outbuf+6);
        } else {
            /* MIC and DEL store 0xFF in EC and RRC.  */
            store_16_be(0xffff, outbuf+4);
            store_16_be(0xffff, outbuf+6);
        }
        store_64_be(ctx->seq_send, outbuf+8);

        memcpy(plain.data, message->value, message->length);
        memcpy(plain.data + message->length, outbuf, 16);

        /* Fill in the output token -- data contents, if any, and
           space for the checksum.  */
        if (message2->length)
            memcpy(outbuf + 16, message2->value, message2->length);

        sum.contents = outbuf + 16 + message2->length;
        sum.length = cksumsize;

        err = krb5_k_make_checksum(context, cksumtype, key,
                                   key_usage, &plain, &sum);
        zap(plain.data, plain.length);
        free(plain.data);
        plain.data = 0;
        if (err) {
            zap(outbuf,bufsize);
            goto error;
        }
        if (sum.length != cksumsize)
            abort();
        memcpy(outbuf + 16 + message2->length, sum.contents, cksumsize);
        krb5_free_checksum_contents(context, &sum);
        sum.contents = 0;
        /* Now that we know we're actually generating the token...  */
        ctx->seq_send++;

        if (toktype == KG_TOK_WRAP_MSG) {
#ifdef CFX_EXERCISE
            rrc = rand() & 0xffff;
            /* If the rotate fails, don't worry about it.  */
            if (gss_krb5int_rotate_left(outbuf+16, bufsize-16,
                                        (bufsize-16) - (rrc % (bufsize - 16))))
                store_16_be(rrc, outbuf+6);
#endif
            /* Fix up EC field.  */
            store_16_be(cksumsize, outbuf+4);
        } else {
            store_16_be(0xffff, outbuf+6);
        }
    } else if (toktype == KG_TOK_MIC_MSG) {
        tok_id = KG2_TOK_MIC_MSG;
        message2 = &empty_message;
        goto wrap_with_checksum;
    } else if (toktype == KG_TOK_DEL_CTX) {
        tok_id = KG2_TOK_DEL_CTX;
        message = message2 = &empty_message;
        goto wrap_with_checksum;
    } else
        abort();

    token->value = outbuf;
    token->length = bufsize;
    return 0;

error:
    gssalloc_free(outbuf);
    token->value = NULL;
    token->length = 0;
    return err;
}

/* message_buffer is an input if SIGN, output if SEAL, and ignored if DEL_CTX
   conf_state is only valid if SEAL. */

OM_uint32
gss_krb5int_unseal_token_v3(krb5_context *contextptr,
                            OM_uint32 *minor_status,
                            krb5_gss_ctx_id_rec *ctx,
                            unsigned char *ptr, unsigned int bodysize,
                            gss_buffer_t message_buffer,
                            int *conf_state, gss_qop_t *qop_state, int toktype)
{
    krb5_context context = *contextptr;
    krb5_data plain;
    uint64_t seqnum;
    size_t ec, rrc;
    int key_usage;
    unsigned char acceptor_flag;
    krb5_checksum sum;
    krb5_error_code err;
    krb5_boolean valid;
    krb5_key key;
    krb5_cksumtype cksumtype;

    if (qop_state)
        *qop_state = GSS_C_QOP_DEFAULT;

    acceptor_flag = ctx->initiate ? FLAG_SENDER_IS_ACCEPTOR : 0;
    key_usage = (toktype == KG_TOK_WRAP_MSG
                 ? (!ctx->initiate
                    ? KG_USAGE_INITIATOR_SEAL
                    : KG_USAGE_ACCEPTOR_SEAL)
                 : (!ctx->initiate
                    ? KG_USAGE_INITIATOR_SIGN
                    : KG_USAGE_ACCEPTOR_SIGN));

    /* Oops.  I wrote this code assuming ptr would be at the start of
       the token header.  */
    ptr -= 2;
    bodysize += 2;

    if (bodysize < 16) {
    defective:
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }
    if ((ptr[2] & FLAG_SENDER_IS_ACCEPTOR) != acceptor_flag) {
        *minor_status = (OM_uint32)G_BAD_DIRECTION;
        return GSS_S_BAD_SIG;
    }

    /* Two things to note here.

       First, we can't really enforce the use of the acceptor's subkey,
       if we're the acceptor; the initiator may have sent messages
       before getting the subkey.  We could probably enforce it if
       we're the initiator.

       Second, if someone tweaks the code to not set the flag telling
       the krb5 library to generate a new subkey in the AP-REP
       message, the MIT library may include a subkey anyways --
       namely, a copy of the AP-REQ subkey, if it was provided.  So
       the initiator may think we wanted a subkey, and set the flag,
       even though we weren't trying to set the subkey.  The "other"
       key, the one not asserted by the acceptor, will have the same
       value in that case, though, so we can just ignore the flag.  */
    if (ctx->have_acceptor_subkey && (ptr[2] & FLAG_ACCEPTOR_SUBKEY)) {
        key = ctx->acceptor_subkey;
        cksumtype = ctx->acceptor_subkey_cksumtype;
    } else {
        key = ctx->subkey;
        cksumtype = ctx->cksumtype;
    }
    assert(key != NULL);

    if (toktype == KG_TOK_WRAP_MSG) {
        if (load_16_be(ptr) != KG2_TOK_WRAP_MSG)
            goto defective;
        if (ptr[3] != 0xff)
            goto defective;
        ec = load_16_be(ptr+4);
        rrc = load_16_be(ptr+6);
        seqnum = load_64_be(ptr+8);
        if (!gss_krb5int_rotate_left(ptr+16, bodysize-16, rrc)) {
        no_mem:
            *minor_status = ENOMEM;
            return GSS_S_FAILURE;
        }
        if (ptr[2] & FLAG_WRAP_CONFIDENTIAL) {
            /* confidentiality */
            krb5_enc_data cipher;
            unsigned char *althdr;

            if (conf_state)
                *conf_state = 1;
            /* Do we have no decrypt_size function?

               For all current cryptosystems, the ciphertext size will
               be larger than the plaintext size.  */
            cipher.enctype = key->keyblock.enctype;
            cipher.ciphertext.length = bodysize - 16;
            cipher.ciphertext.data = (char *)ptr + 16;
            plain.length = bodysize - 16;
            plain.data = gssalloc_malloc(plain.length);
            if (plain.data == NULL)
                goto no_mem;
            err = krb5_k_decrypt(context, key, key_usage, 0,
                                 &cipher, &plain);
            if (err) {
                gssalloc_free(plain.data);
                goto error;
            }
            /* Don't use bodysize here!  Use the fact that
               cipher.ciphertext.length has been adjusted to the
               correct length.  */
            althdr = (unsigned char *)plain.data + plain.length - 16;
            if (load_16_be(althdr) != KG2_TOK_WRAP_MSG
                || althdr[2] != ptr[2]
                || althdr[3] != ptr[3]
                || memcmp(althdr+8, ptr+8, 8)) {
                free(plain.data);
                goto defective;
            }
            message_buffer->value = plain.data;
            message_buffer->length = plain.length - ec - 16;
            if(message_buffer->length == 0) {
                gssalloc_free(message_buffer->value);
                message_buffer->value = NULL;
            }
        } else {
            size_t cksumsize;

            err = krb5_c_checksum_length(context, cksumtype, &cksumsize);
            if (err)
                goto error;

            /* no confidentiality */
            if (conf_state)
                *conf_state = 0;
            if (ec + 16 < ec)
                /* overflow check */
                goto defective;
            if (ec + 16 > bodysize)
                goto defective;
            /* We have: header | msg | cksum.
               We need cksum(msg | header).
               Rotate the first two.  */
            store_16_be(0, ptr+4);
            store_16_be(0, ptr+6);
            plain = make_data(ptr, bodysize - ec);
            if (!gss_krb5int_rotate_left(ptr, bodysize-ec, 16))
                goto no_mem;
            sum.length = ec;
            if (sum.length != cksumsize) {
                *minor_status = 0;
                return GSS_S_BAD_SIG;
            }
            sum.contents = ptr+bodysize-ec;
            sum.checksum_type = cksumtype;
            err = krb5_k_verify_checksum(context, key, key_usage,
                                         &plain, &sum, &valid);
            if (err)
                goto error;
            if (!valid) {
                *minor_status = 0;
                return GSS_S_BAD_SIG;
            }
            message_buffer->length = plain.length - 16;
            message_buffer->value = gssalloc_malloc(message_buffer->length);
            if (message_buffer->value == NULL)
                goto no_mem;
            memcpy(message_buffer->value, plain.data, message_buffer->length);
        }
        err = g_seqstate_check(ctx->seqstate, seqnum);
        *minor_status = 0;
        return err;
    } else if (toktype == KG_TOK_MIC_MSG) {
        /* wrap token, no confidentiality */
        if (load_16_be(ptr) != KG2_TOK_MIC_MSG)
            goto defective;
    verify_mic_1:
        if (ptr[3] != 0xff)
            goto defective;
        if (load_32_be(ptr+4) != 0xffffffffL)
            goto defective;
        seqnum = load_64_be(ptr+8);
        plain.length = message_buffer->length + 16;
        plain.data = malloc(plain.length);
        if (plain.data == NULL)
            goto no_mem;
        if (message_buffer->length)
            memcpy(plain.data, message_buffer->value, message_buffer->length);
        memcpy(plain.data + message_buffer->length, ptr, 16);
        sum.length = bodysize - 16;
        sum.contents = ptr + 16;
        sum.checksum_type = cksumtype;
        err = krb5_k_verify_checksum(context, key, key_usage,
                                     &plain, &sum, &valid);
        free(plain.data);
        plain.data = NULL;
        if (err) {
        error:
            *minor_status = err;
            save_error_info(*minor_status, context);
            return GSS_S_BAD_SIG; /* XXX */
        }
        if (!valid) {
            *minor_status = 0;
            return GSS_S_BAD_SIG;
        }
        err = g_seqstate_check(ctx->seqstate, seqnum);
        *minor_status = 0;
        return err;
    } else if (toktype == KG_TOK_DEL_CTX) {
        if (load_16_be(ptr) != KG2_TOK_DEL_CTX)
            goto defective;
        message_buffer = (gss_buffer_t)&empty_message;
        goto verify_mic_1;
    } else {
        goto defective;
    }
}
