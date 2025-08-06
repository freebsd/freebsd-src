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
    unsigned char *outbuf = NULL;
    krb5_error_code err;
    int key_usage;
    unsigned char acceptor_flag;
    const gss_buffer_desc *message2 = message;
#ifdef CFX_EXERCISE
    size_t rrc;
#endif
    size_t ec;
    unsigned short tok_id;
    krb5_checksum sum = { 0 };
    krb5_key key;
    krb5_cksumtype cksumtype;
    krb5_data plain = empty_data();

    token->value = NULL;
    token->length = 0;

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
        krb5_enc_data cipher;
        size_t ec_max;
        size_t encrypt_size;

        /* 300: Adds some slop.  */
        if (SIZE_MAX - 300 < message->length) {
            err = ENOMEM;
            goto cleanup;
        }
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
            goto cleanup;

        /* Get size of ciphertext.  */
        encrypt_size = krb5_encrypt_size(plain.length, key->keyblock.enctype);
        if (encrypt_size > SIZE_MAX / 2) {
            err = ENOMEM;
            goto cleanup;
        }
        bufsize = 16 + encrypt_size;
        /* Allocate space for header plus encrypted data.  */
        outbuf = gssalloc_malloc(bufsize);
        if (outbuf == NULL) {
            err = ENOMEM;
            goto cleanup;
        }

        /* TOK_ID */
        store_16_be(KG2_TOK_WRAP_MSG, outbuf);
        /* flags */
        outbuf[2] = (acceptor_flag | FLAG_WRAP_CONFIDENTIAL |
                     (ctx->have_acceptor_subkey ? FLAG_ACCEPTOR_SUBKEY : 0));
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
        if (err)
            goto cleanup;

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
        size_t cksumsize;

        /* Here, message is the application-supplied data; message2 is
           what goes into the output token.  They may be the same, or
           message2 may be empty (for MIC).  */

        tok_id = KG2_TOK_WRAP_MSG;

    wrap_with_checksum:
        err = alloc_data(&plain, message->length + 16);
        if (err)
            goto cleanup;

        err = krb5_c_checksum_length(context, cksumtype, &cksumsize);
        if (err)
            goto cleanup;

        assert(cksumsize <= 0xffff);

        bufsize = 16 + message2->length + cksumsize;
        outbuf = gssalloc_malloc(bufsize);
        if (outbuf == NULL) {
            err = ENOMEM;
            goto cleanup;
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

        err = krb5_k_make_checksum(context, cksumtype, key,
                                   key_usage, &plain, &sum);
        if (err) {
            zap(outbuf,bufsize);
            goto cleanup;
        }
        if (sum.length != cksumsize)
            abort();
        memcpy(outbuf + 16 + message2->length, sum.contents, cksumsize);
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
    outbuf = NULL;
    err = 0;

cleanup:
    krb5_free_checksum_contents(context, &sum);
    zapfree(plain.data, plain.length);
    gssalloc_free(outbuf);
    return err;
}
