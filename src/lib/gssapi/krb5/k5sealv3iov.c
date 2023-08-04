/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/k5sealv3iov.c */
/*
 * Copyright 2008 by the Massachusetts Institute of Technology.
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
#include "gssapiP_krb5.h"

krb5_error_code
gss_krb5int_make_seal_token_v3_iov(krb5_context context,
                                   krb5_gss_ctx_id_rec *ctx,
                                   int conf_req_flag,
                                   int *conf_state,
                                   gss_iov_buffer_desc *iov,
                                   int iov_count,
                                   int toktype)
{
    krb5_error_code code = 0;
    gss_iov_buffer_t header;
    gss_iov_buffer_t padding;
    gss_iov_buffer_t trailer;
    unsigned char acceptor_flag;
    unsigned short tok_id;
    unsigned char *outbuf = NULL;
    unsigned char *tbuf = NULL;
    int key_usage;
    size_t rrc = 0;
    unsigned int  gss_headerlen, gss_trailerlen;
    krb5_key key;
    krb5_cksumtype cksumtype;
    size_t data_length, assoc_data_length;

    assert(ctx->proto == 1);

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
    assert(cksumtype != 0);

    kg_iov_msglen(iov, iov_count, &data_length, &assoc_data_length);

    header = kg_locate_header_iov(iov, iov_count, toktype);
    if (header == NULL)
        return EINVAL;

    padding = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_PADDING);
    if (padding != NULL)
        padding->buffer.length = 0;

    trailer = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);

    if (toktype == KG_TOK_WRAP_MSG && conf_req_flag) {
        unsigned int k5_headerlen, k5_trailerlen, k5_padlen;
        size_t ec = 0;
        size_t conf_data_length = data_length - assoc_data_length;

        code = krb5_c_crypto_length(context, key->keyblock.enctype,
                                    KRB5_CRYPTO_TYPE_HEADER, &k5_headerlen);
        if (code != 0)
            goto cleanup;

        code = krb5_c_padding_length(context, key->keyblock.enctype,
                                     conf_data_length + 16 /* E(Header) */, &k5_padlen);
        if (code != 0)
            goto cleanup;

        if (k5_padlen == 0 && (ctx->gss_flags & GSS_C_DCE_STYLE)) {
            /* Windows rejects AEAD tokens with non-zero EC */
            code = krb5_c_block_size(context, key->keyblock.enctype, &ec);
            if (code != 0)
                goto cleanup;
        } else
            ec = k5_padlen;

        code = krb5_c_crypto_length(context, key->keyblock.enctype,
                                    KRB5_CRYPTO_TYPE_TRAILER, &k5_trailerlen);
        if (code != 0)
            goto cleanup;

        gss_headerlen = 16 /* Header */ + k5_headerlen;
        gss_trailerlen = ec + 16 /* E(Header) */ + k5_trailerlen;

        if (trailer == NULL) {
            rrc = gss_trailerlen;
            /* Workaround for Windows bug where it rotates by EC + RRC */
            if (ctx->gss_flags & GSS_C_DCE_STYLE)
                rrc -= ec;
            gss_headerlen += gss_trailerlen;
        }

        if (header->type & GSS_IOV_BUFFER_FLAG_ALLOCATE) {
            code = kg_allocate_iov(header, (size_t) gss_headerlen);
        } else if (header->buffer.length < gss_headerlen)
            code = KRB5_BAD_MSIZE;
        if (code != 0)
            goto cleanup;
        outbuf = (unsigned char *)header->buffer.value;
        header->buffer.length = (size_t) gss_headerlen;

        if (trailer != NULL) {
            if (trailer->type & GSS_IOV_BUFFER_FLAG_ALLOCATE)
                code = kg_allocate_iov(trailer, (size_t) gss_trailerlen);
            else if (trailer->buffer.length < gss_trailerlen)
                code = KRB5_BAD_MSIZE;
            if (code != 0)
                goto cleanup;
            trailer->buffer.length = (size_t) gss_trailerlen;
        }

        /* TOK_ID */
        store_16_be(KG2_TOK_WRAP_MSG, outbuf);
        /* flags */
        outbuf[2] = (acceptor_flag | FLAG_WRAP_CONFIDENTIAL |
                     (ctx->have_acceptor_subkey ? FLAG_ACCEPTOR_SUBKEY : 0));
        /* filler */
        outbuf[3] = 0xFF;
        /* EC */
        store_16_be(ec, outbuf + 4);
        /* RRC */
        store_16_be(0, outbuf + 6);
        store_64_be(ctx->seq_send, outbuf + 8);

        /* EC | copy of header to be encrypted, located in (possibly rotated) trailer */
        if (trailer == NULL)
            tbuf = (unsigned char *)header->buffer.value + 16; /* Header */
        else
            tbuf = (unsigned char *)trailer->buffer.value;

        memset(tbuf, 0xFF, ec);
        memcpy(tbuf + ec, header->buffer.value, 16);

        code = kg_encrypt_iov(context, ctx->proto,
                              ((ctx->gss_flags & GSS_C_DCE_STYLE) != 0),
                              ec, rrc, key, key_usage, 0, iov, iov_count);
        if (code != 0)
            goto cleanup;

        /* RRC */
        store_16_be(rrc, outbuf + 6);

        ctx->seq_send++;
    } else if (toktype == KG_TOK_WRAP_MSG && !conf_req_flag) {
        tok_id = KG2_TOK_WRAP_MSG;

    wrap_with_checksum:

        gss_headerlen = 16;

        code = krb5_c_crypto_length(context, key->keyblock.enctype,
                                    KRB5_CRYPTO_TYPE_CHECKSUM,
                                    &gss_trailerlen);
        if (code != 0)
            goto cleanup;

        assert(gss_trailerlen <= 0xFFFF);

        if (trailer == NULL) {
            rrc = gss_trailerlen;
            gss_headerlen += gss_trailerlen;
        }

        if (header->type & GSS_IOV_BUFFER_FLAG_ALLOCATE)
            code = kg_allocate_iov(header, (size_t) gss_headerlen);
        else if (header->buffer.length < gss_headerlen)
            code = KRB5_BAD_MSIZE;
        if (code != 0)
            goto cleanup;
        outbuf = (unsigned char *)header->buffer.value;
        header->buffer.length = (size_t) gss_headerlen;

        if (trailer != NULL) {
            if (trailer->type & GSS_IOV_BUFFER_FLAG_ALLOCATE)
                code = kg_allocate_iov(trailer, (size_t) gss_trailerlen);
            else if (trailer->buffer.length < gss_trailerlen)
                code = KRB5_BAD_MSIZE;
            if (code != 0)
                goto cleanup;
            trailer->buffer.length = (size_t) gss_trailerlen;
        }

        /* TOK_ID */
        store_16_be(tok_id, outbuf);
        /* flags */
        outbuf[2] = (acceptor_flag
                     | (ctx->have_acceptor_subkey ? FLAG_ACCEPTOR_SUBKEY : 0));
        /* filler */
        outbuf[3] = 0xFF;
        if (toktype == KG_TOK_WRAP_MSG) {
            /* Use 0 for checksum calculation, substitute
             * checksum length later.
             */
            /* EC */
            store_16_be(0, outbuf + 4);
            /* RRC */
            store_16_be(0, outbuf + 6);
        } else {
            /* MIC and DEL store 0xFF in EC and RRC */
            store_16_be(0xFFFF, outbuf + 4);
            store_16_be(0xFFFF, outbuf + 6);
        }
        store_64_be(ctx->seq_send, outbuf + 8);

        code = kg_make_checksum_iov_v3(context, cksumtype,
                                       rrc, key, key_usage,
                                       iov, iov_count, toktype);
        if (code != 0)
            goto cleanup;

        ctx->seq_send++;

        if (toktype == KG_TOK_WRAP_MSG) {
            /* Fix up EC field */
            store_16_be(gss_trailerlen, outbuf + 4);
            /* Fix up RRC field */
            store_16_be(rrc, outbuf + 6);
        }
    } else if (toktype == KG_TOK_MIC_MSG) {
        tok_id = KG2_TOK_MIC_MSG;
        trailer = NULL;
        goto wrap_with_checksum;
    } else if (toktype == KG_TOK_DEL_CTX) {
        tok_id = KG2_TOK_DEL_CTX;
        goto wrap_with_checksum;
    } else {
        abort();
    }

    code = 0;
    if (conf_state != NULL)
        *conf_state = conf_req_flag;

cleanup:
    if (code != 0)
        kg_release_iov(iov, iov_count);

    return code;
}

OM_uint32
gss_krb5int_unseal_v3_iov(krb5_context context,
                          OM_uint32 *minor_status,
                          krb5_gss_ctx_id_rec *ctx,
                          gss_iov_buffer_desc *iov,
                          int iov_count,
                          int *conf_state,
                          gss_qop_t *qop_state,
                          int toktype)
{
    OM_uint32 code;
    gss_iov_buffer_t header;
    gss_iov_buffer_t padding;
    gss_iov_buffer_t trailer;
    unsigned char acceptor_flag;
    unsigned char *ptr = NULL;
    int key_usage;
    size_t rrc, ec;
    size_t data_length, assoc_data_length;
    krb5_key key;
    uint64_t seqnum;
    krb5_boolean valid;
    krb5_cksumtype cksumtype;
    int conf_flag = 0;

    if (qop_state != NULL)
        *qop_state = GSS_C_QOP_DEFAULT;

    header = kg_locate_header_iov(iov, iov_count, toktype);
    assert(header != NULL);

    padding = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_PADDING);
    if (padding != NULL && padding->buffer.length != 0)
        return GSS_S_DEFECTIVE_TOKEN;

    trailer = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);

    acceptor_flag = ctx->initiate ? FLAG_SENDER_IS_ACCEPTOR : 0;
    key_usage = (toktype == KG_TOK_WRAP_MSG
                 ? (!ctx->initiate
                    ? KG_USAGE_INITIATOR_SEAL
                    : KG_USAGE_ACCEPTOR_SEAL)
                 : (!ctx->initiate
                    ? KG_USAGE_INITIATOR_SIGN
                    : KG_USAGE_ACCEPTOR_SIGN));

    kg_iov_msglen(iov, iov_count, &data_length, &assoc_data_length);

    ptr = (unsigned char *)header->buffer.value;

    if (header->buffer.length < 16) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if ((ptr[2] & FLAG_SENDER_IS_ACCEPTOR) != acceptor_flag) {
        *minor_status = (OM_uint32)G_BAD_DIRECTION;
        return GSS_S_BAD_SIG;
    }

    if (ctx->have_acceptor_subkey && (ptr[2] & FLAG_ACCEPTOR_SUBKEY)) {
        key = ctx->acceptor_subkey;
        cksumtype = ctx->acceptor_subkey_cksumtype;
    } else {
        key = ctx->subkey;
        cksumtype = ctx->cksumtype;
    }
    assert(key != NULL);


    if (toktype == KG_TOK_WRAP_MSG) {
        unsigned int k5_trailerlen;

        if (load_16_be(ptr) != KG2_TOK_WRAP_MSG)
            goto defective;
        conf_flag = ((ptr[2] & FLAG_WRAP_CONFIDENTIAL) != 0);
        if (ptr[3] != 0xFF)
            goto defective;
        ec = load_16_be(ptr + 4);
        rrc = load_16_be(ptr + 6);
        seqnum = load_64_be(ptr + 8);

        code = krb5_c_crypto_length(context, key->keyblock.enctype,
                                    conf_flag ? KRB5_CRYPTO_TYPE_TRAILER :
                                    KRB5_CRYPTO_TYPE_CHECKSUM,
                                    &k5_trailerlen);
        if (code != 0) {
            *minor_status = code;
            return GSS_S_FAILURE;
        }

        /* Deal with RRC */
        if (trailer == NULL) {
            size_t desired_rrc = k5_trailerlen;

            if (conf_flag) {
                desired_rrc += 16; /* E(Header) */

                if ((ctx->gss_flags & GSS_C_DCE_STYLE) == 0)
                    desired_rrc += ec;
            }

            /* According to MS, we only need to deal with a fixed RRC for DCE */
            if (rrc != desired_rrc)
                goto defective;
        } else if (rrc != 0) {
            /* Should have been rotated by kg_unseal_stream_iov() */
            goto defective;
        }

        if (conf_flag) {
            unsigned char *althdr;

            /* Decrypt */
            code = kg_decrypt_iov(context, ctx->proto,
                                  ((ctx->gss_flags & GSS_C_DCE_STYLE) != 0),
                                  ec, rrc,
                                  key, key_usage, 0, iov, iov_count);
            if (code != 0) {
                *minor_status = code;
                return GSS_S_BAD_SIG;
            }

            /* Validate header integrity */
            if (trailer == NULL)
                althdr = (unsigned char *)header->buffer.value + 16 + ec;
            else
                althdr = (unsigned char *)trailer->buffer.value + ec;

            if (load_16_be(althdr) != KG2_TOK_WRAP_MSG
                || althdr[2] != ptr[2]
                || althdr[3] != ptr[3]
                || memcmp(althdr + 8, ptr + 8, 8) != 0) {
                *minor_status = 0;
                return GSS_S_BAD_SIG;
            }
        } else {
            /* Verify checksum: note EC is checksum size here, not padding */
            if (ec != k5_trailerlen)
                goto defective;

            /* Zero EC, RRC before computing checksum */
            store_16_be(0, ptr + 4);
            store_16_be(0, ptr + 6);

            code = kg_verify_checksum_iov_v3(context, cksumtype, rrc,
                                             key, key_usage,
                                             iov, iov_count, toktype, &valid);
            if (code != 0 || valid == FALSE) {
                *minor_status = code;
                return GSS_S_BAD_SIG;
            }
        }

        code = g_seqstate_check(ctx->seqstate, seqnum);
    } else if (toktype == KG_TOK_MIC_MSG) {
        if (load_16_be(ptr) != KG2_TOK_MIC_MSG)
            goto defective;

    verify_mic_1:
        if (ptr[3] != 0xFF)
            goto defective;
        seqnum = load_64_be(ptr + 8);

        /* For MIC tokens, the GSS header and checksum are in the same buffer.
         * Fake up an RRC so that the checksum is expected in the header. */
        rrc = (trailer != NULL) ? 0 : header->buffer.length - 16;
        code = kg_verify_checksum_iov_v3(context, cksumtype, rrc,
                                         key, key_usage,
                                         iov, iov_count, toktype, &valid);
        if (code != 0 || valid == FALSE) {
            *minor_status = code;
            return GSS_S_BAD_SIG;
        }
        code = g_seqstate_check(ctx->seqstate, seqnum);
    } else if (toktype == KG_TOK_DEL_CTX) {
        if (load_16_be(ptr) != KG2_TOK_DEL_CTX)
            goto defective;
        goto verify_mic_1;
    } else {
        goto defective;
    }

    *minor_status = 0;

    if (conf_state != NULL)
        *conf_state = conf_flag;

    return code;

defective:
    *minor_status = 0;

    return GSS_S_DEFECTIVE_TOKEN;
}
