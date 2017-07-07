/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/k5sealiov.c */
/*
 * Copyright 2008, 2009 by the Massachusetts Institute of Technology.
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

static krb5_error_code
make_seal_token_v1_iov(krb5_context context,
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
    krb5_checksum md5cksum;
    krb5_checksum cksum;
    size_t k5_headerlen = 0, k5_trailerlen = 0;
    size_t data_length = 0, assoc_data_length = 0;
    size_t tmsglen = 0, tlen;
    unsigned char *ptr;
    krb5_keyusage sign_usage = KG_USAGE_SIGN;

    md5cksum.length = cksum.length = 0;
    md5cksum.contents = cksum.contents = NULL;

    header = kg_locate_header_iov(iov, iov_count, toktype);
    if (header == NULL)
        return EINVAL;

    padding = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_PADDING);
    if (padding == NULL && toktype == KG_TOK_WRAP_MSG &&
        (ctx->gss_flags & GSS_C_DCE_STYLE) == 0)
        return EINVAL;

    trailer = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);
    if (trailer != NULL)
        trailer->buffer.length = 0;

    /* Determine confounder length */
    if (toktype == KG_TOK_WRAP_MSG || conf_req_flag)
        k5_headerlen = kg_confounder_size(context, ctx->enc->keyblock.enctype);

    /* Check padding length */
    if (toktype == KG_TOK_WRAP_MSG) {
        size_t k5_padlen = (ctx->sealalg == SEAL_ALG_MICROSOFT_RC4) ? 1 : 8;
        size_t gss_padlen;
        size_t conf_data_length;

        kg_iov_msglen(iov, iov_count, &data_length, &assoc_data_length);
        conf_data_length = k5_headerlen + data_length - assoc_data_length;

        if (k5_padlen == 1)
            gss_padlen = 1; /* one byte to indicate one byte of padding */
        else
            gss_padlen = k5_padlen - (conf_data_length % k5_padlen);

        if (ctx->gss_flags & GSS_C_DCE_STYLE) {
            /* DCE will pad the actual data itself; padding buffer optional and will be zeroed */
            gss_padlen = 0;

            if (conf_data_length % k5_padlen)
                code = KRB5_BAD_MSIZE;
        } else if (padding->type & GSS_IOV_BUFFER_FLAG_ALLOCATE) {
            code = kg_allocate_iov(padding, gss_padlen);
        } else if (padding->buffer.length < gss_padlen) {
            code = KRB5_BAD_MSIZE;
        }
        if (code != 0)
            goto cleanup;

        /* Initialize padding buffer to pad itself */
        if (padding != NULL) {
            padding->buffer.length = gss_padlen;
            memset(padding->buffer.value, (int)gss_padlen, gss_padlen);
        }

        if (ctx->gss_flags & GSS_C_DCE_STYLE)
            tmsglen = k5_headerlen; /* confounder length */
        else
            tmsglen = conf_data_length + padding->buffer.length;
    }

    /* Determine token size */
    tlen = g_token_size(ctx->mech_used, 14 + ctx->cksum_size + tmsglen);

    k5_headerlen += tlen - tmsglen;

    if (header->type & GSS_IOV_BUFFER_FLAG_ALLOCATE)
        code = kg_allocate_iov(header, k5_headerlen);
    else if (header->buffer.length < k5_headerlen)
        code = KRB5_BAD_MSIZE;
    if (code != 0)
        goto cleanup;

    header->buffer.length = k5_headerlen;

    ptr = (unsigned char *)header->buffer.value;
    g_make_token_header(ctx->mech_used, 14 + ctx->cksum_size + tmsglen, &ptr, toktype);

    /* 0..1 SIGN_ALG */
    store_16_le(ctx->signalg, &ptr[0]);

    /* 2..3 SEAL_ALG or Filler */
    if (toktype == KG_TOK_WRAP_MSG && conf_req_flag) {
        store_16_le(ctx->sealalg, &ptr[2]);
    } else {
        /* No seal */
        ptr[2] = 0xFF;
        ptr[3] = 0xFF;
    }

    /* 4..5 Filler */
    ptr[4] = 0xFF;
    ptr[5] = 0xFF;

    /* pad the plaintext, encrypt if needed, and stick it in the token */

    /* initialize the checksum */
    switch (ctx->signalg) {
    case SGN_ALG_DES_MAC_MD5:
    case SGN_ALG_MD2_5:
        md5cksum.checksum_type = CKSUMTYPE_RSA_MD5;
        break;
    case SGN_ALG_HMAC_SHA1_DES3_KD:
        md5cksum.checksum_type = CKSUMTYPE_HMAC_SHA1_DES3;
        break;
    case SGN_ALG_HMAC_MD5:
        md5cksum.checksum_type = CKSUMTYPE_HMAC_MD5_ARCFOUR;
        if (toktype != KG_TOK_WRAP_MSG)
            sign_usage = 15;
        break;
    default:
    case SGN_ALG_DES_MAC:
        abort ();
    }

    code = krb5_c_checksum_length(context, md5cksum.checksum_type, &k5_trailerlen);
    if (code != 0)
        goto cleanup;
    md5cksum.length = k5_trailerlen;

    if (k5_headerlen != 0 && toktype == KG_TOK_WRAP_MSG) {
        code = kg_make_confounder(context, ctx->enc->keyblock.enctype,
                                  ptr + 14 + ctx->cksum_size);
        if (code != 0)
            goto cleanup;
    }

    /* compute the checksum */
    code = kg_make_checksum_iov_v1(context, md5cksum.checksum_type,
                                   ctx->cksum_size, ctx->seq, ctx->enc,
                                   sign_usage, iov, iov_count, toktype,
                                   &md5cksum);
    if (code != 0)
        goto cleanup;

    switch (ctx->signalg) {
    case SGN_ALG_DES_MAC_MD5:
    case SGN_ALG_3:
        code = kg_encrypt_inplace(context, ctx->seq, KG_USAGE_SEAL,
                                  (g_OID_equal(ctx->mech_used,
                                               gss_mech_krb5_old) ?
                                   ctx->seq->keyblock.contents : NULL),
                                  md5cksum.contents, 16);
        if (code != 0)
            goto cleanup;

        cksum.length = ctx->cksum_size;
        cksum.contents = md5cksum.contents + 16 - cksum.length;

        memcpy(ptr + 14, cksum.contents, cksum.length);
        break;
    case SGN_ALG_HMAC_SHA1_DES3_KD:
        assert(md5cksum.length == ctx->cksum_size);
        memcpy(ptr + 14, md5cksum.contents, md5cksum.length);
        break;
    case SGN_ALG_HMAC_MD5:
        memcpy(ptr + 14, md5cksum.contents, ctx->cksum_size);
        break;
    }

    /* create the seq_num */
    code = kg_make_seq_num(context, ctx->seq, ctx->initiate ? 0 : 0xFF,
                           (OM_uint32)ctx->seq_send, ptr + 14, ptr + 6);
    if (code != 0)
        goto cleanup;

    if (conf_req_flag) {
        if (ctx->sealalg == SEAL_ALG_MICROSOFT_RC4) {
            unsigned char bigend_seqnum[4];
            krb5_keyblock *enc_key;
            size_t i;

            store_32_be(ctx->seq_send, bigend_seqnum);

            code = krb5_k_key_keyblock(context, ctx->enc, &enc_key);
            if (code != 0)
                goto cleanup;

            assert(enc_key->length == 16);

            for (i = 0; i < enc_key->length; i++)
                ((char *)enc_key->contents)[i] ^= 0xF0;

            code = kg_arcfour_docrypt_iov(context, enc_key, 0,
                                          bigend_seqnum, 4,
                                          iov, iov_count);
            krb5_free_keyblock(context, enc_key);
        } else {
            code = kg_encrypt_iov(context, ctx->proto,
                                  ((ctx->gss_flags & GSS_C_DCE_STYLE) != 0),
                                  0 /*EC*/, 0 /*RRC*/,
                                  ctx->enc, KG_USAGE_SEAL, NULL,
                                  iov, iov_count);
        }
        if (code != 0)
            goto cleanup;
    }

    ctx->seq_send++;
    ctx->seq_send &= 0xFFFFFFFFL;

    code = 0;

    if (conf_state != NULL)
        *conf_state = conf_req_flag;

cleanup:
    if (code != 0)
        kg_release_iov(iov, iov_count);
    krb5_free_checksum_contents(context, &md5cksum);

    return code;
}

OM_uint32
kg_seal_iov(OM_uint32 *minor_status,
            gss_ctx_id_t context_handle,
            int conf_req_flag,
            gss_qop_t qop_req,
            int *conf_state,
            gss_iov_buffer_desc *iov,
            int iov_count,
            int toktype)
{
    krb5_gss_ctx_id_rec *ctx;
    krb5_error_code code;
    krb5_context context;

    if (qop_req != 0) {
        *minor_status = (OM_uint32)G_UNKNOWN_QOP;
        return GSS_S_BAD_QOP;
    }

    ctx = (krb5_gss_ctx_id_rec *)context_handle;
    if (ctx->terminated || !ctx->established) {
        *minor_status = KG_CTX_INCOMPLETE;
        return GSS_S_NO_CONTEXT;
    }

    if (conf_req_flag && kg_integ_only_iov(iov, iov_count)) {
        /* may be more sensible to return an error here */
        conf_req_flag = FALSE;
    }

    context = ctx->k5_context;
    switch (ctx->proto) {
    case 0:
        code = make_seal_token_v1_iov(context, ctx, conf_req_flag,
                                      conf_state, iov, iov_count, toktype);
        break;
    case 1:
        code = gss_krb5int_make_seal_token_v3_iov(context, ctx, conf_req_flag,
                                                  conf_state, iov, iov_count, toktype);
        break;
    default:
        code = G_UNKNOWN_QOP;
        break;
    }

    if (code != 0) {
        *minor_status = code;
        save_error_info(*minor_status, context);
        return GSS_S_FAILURE;
    }

    *minor_status = 0;

    return GSS_S_COMPLETE;
}

#define INIT_IOV_DATA(_iov)     do { (_iov)->buffer.value = NULL;       \
        (_iov)->buffer.length = 0; }                                    \
    while (0)

OM_uint32
kg_seal_iov_length(OM_uint32 *minor_status,
                   gss_ctx_id_t context_handle,
                   int conf_req_flag,
                   gss_qop_t qop_req,
                   int *conf_state,
                   gss_iov_buffer_desc *iov,
                   int iov_count,
                   int toktype)
{
    krb5_gss_ctx_id_rec *ctx;
    gss_iov_buffer_t header, trailer, padding;
    size_t data_length, assoc_data_length;
    size_t gss_headerlen, gss_padlen, gss_trailerlen;
    unsigned int k5_headerlen = 0, k5_trailerlen = 0, k5_padlen = 0;
    krb5_error_code code;
    krb5_context context;
    int dce_or_mic;

    if (qop_req != GSS_C_QOP_DEFAULT) {
        *minor_status = (OM_uint32)G_UNKNOWN_QOP;
        return GSS_S_BAD_QOP;
    }

    ctx = (krb5_gss_ctx_id_rec *)context_handle;
    if (!ctx->established) {
        *minor_status = KG_CTX_INCOMPLETE;
        return GSS_S_NO_CONTEXT;
    }

    header = kg_locate_header_iov(iov, iov_count, toktype);
    if (header == NULL) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }
    INIT_IOV_DATA(header);

    trailer = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);
    if (trailer != NULL) {
        INIT_IOV_DATA(trailer);
    }

    /* MIC tokens and DCE-style wrap tokens have similar length considerations:
     * no padding, and the framing surrounds the header only, not the data. */
    dce_or_mic = ((ctx->gss_flags & GSS_C_DCE_STYLE) != 0 ||
                  toktype == KG_TOK_MIC_MSG);

    /* For CFX, EC is used instead of padding, and is placed in header or trailer */
    padding = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_PADDING);
    if (padding == NULL) {
        if (conf_req_flag && ctx->proto == 0 && !dce_or_mic) {
            *minor_status = EINVAL;
            return GSS_S_FAILURE;
        }
    } else {
        INIT_IOV_DATA(padding);
    }

    kg_iov_msglen(iov, iov_count, &data_length, &assoc_data_length);

    if (conf_req_flag && kg_integ_only_iov(iov, iov_count))
        conf_req_flag = FALSE;

    context = ctx->k5_context;

    gss_headerlen = gss_padlen = gss_trailerlen = 0;

    if (ctx->proto == 1) {
        krb5_key key;
        krb5_enctype enctype;
        size_t ec;

        key = (ctx->have_acceptor_subkey) ? ctx->acceptor_subkey : ctx->subkey;
        enctype = key->keyblock.enctype;

        code = krb5_c_crypto_length(context, enctype,
                                    conf_req_flag ?
                                    KRB5_CRYPTO_TYPE_TRAILER : KRB5_CRYPTO_TYPE_CHECKSUM,
                                    &k5_trailerlen);
        if (code != 0) {
            *minor_status = code;
            return GSS_S_FAILURE;
        }

        if (conf_req_flag) {
            code = krb5_c_crypto_length(context, enctype, KRB5_CRYPTO_TYPE_HEADER, &k5_headerlen);
            if (code != 0) {
                *minor_status = code;
                return GSS_S_FAILURE;
            }
        }

        gss_headerlen = 16; /* Header */
        if (conf_req_flag) {
            gss_headerlen += k5_headerlen; /* Kerb-Header */
            gss_trailerlen = 16 /* E(Header) */ + k5_trailerlen; /* Kerb-Trailer */

            code = krb5_c_padding_length(context, enctype,
                                         data_length - assoc_data_length + 16 /* E(Header) */, &k5_padlen);
            if (code != 0) {
                *minor_status = code;
                return GSS_S_FAILURE;
            }

            if (k5_padlen == 0 && dce_or_mic) {
                /* Windows rejects AEAD tokens with non-zero EC */
                code = krb5_c_block_size(context, enctype, &ec);
                if (code != 0) {
                    *minor_status = code;
                    return GSS_S_FAILURE;
                }
            } else
                ec = k5_padlen;

            gss_trailerlen += ec;
        } else {
            gss_trailerlen = k5_trailerlen; /* Kerb-Checksum */
        }
    } else if (!dce_or_mic) {
        k5_padlen = (ctx->sealalg == SEAL_ALG_MICROSOFT_RC4) ? 1 : 8;

        if (k5_padlen == 1)
            gss_padlen = 1;
        else
            gss_padlen = k5_padlen - ((data_length - assoc_data_length) % k5_padlen);
    }

    data_length += gss_padlen;

    if (ctx->proto == 0) {
        /* Header | Checksum | Confounder | Data | Pad */
        size_t data_size;

        k5_headerlen = kg_confounder_size(context, ctx->enc->keyblock.enctype);

        data_size = 14 /* Header */ + ctx->cksum_size + k5_headerlen;

        if (!dce_or_mic)
            data_size += data_length;

        gss_headerlen = g_token_size(ctx->mech_used, data_size);

        /* g_token_size() will include data_size as well as the overhead, so
         * subtract data_length just to get the overhead (ie. token size) */
        if (!dce_or_mic)
            gss_headerlen -= data_length;
    }

    if (minor_status != NULL)
        *minor_status = 0;

    if (trailer == NULL)
        gss_headerlen += gss_trailerlen;
    else
        trailer->buffer.length = gss_trailerlen;

    assert(gss_padlen == 0 || padding != NULL);

    if (padding != NULL)
        padding->buffer.length = gss_padlen;

    header->buffer.length = gss_headerlen;

    if (conf_state != NULL)
        *conf_state = conf_req_flag;

    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_wrap_iov(OM_uint32 *minor_status,
                  gss_ctx_id_t context_handle,
                  int conf_req_flag,
                  gss_qop_t qop_req,
                  int *conf_state,
                  gss_iov_buffer_desc *iov,
                  int iov_count)
{
    OM_uint32 major_status;

    major_status = kg_seal_iov(minor_status, context_handle, conf_req_flag,
                               qop_req, conf_state,
                               iov, iov_count, KG_TOK_WRAP_MSG);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_wrap_iov_length(OM_uint32 *minor_status,
                         gss_ctx_id_t context_handle,
                         int conf_req_flag,
                         gss_qop_t qop_req,
                         int *conf_state,
                         gss_iov_buffer_desc *iov,
                         int iov_count)
{
    OM_uint32 major_status;

    major_status = kg_seal_iov_length(minor_status, context_handle,
                                      conf_req_flag, qop_req, conf_state, iov,
                                      iov_count, KG_TOK_WRAP_MSG);
    return major_status;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_get_mic_iov(OM_uint32 *minor_status,
                     gss_ctx_id_t context_handle,
                     gss_qop_t qop_req,
                     gss_iov_buffer_desc *iov,
                     int iov_count)
{
    OM_uint32 major_status;

    major_status = kg_seal_iov(minor_status, context_handle, FALSE,
                               qop_req, NULL,
                               iov, iov_count, KG_TOK_MIC_MSG);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_get_mic_iov_length(OM_uint32 *minor_status,
                            gss_ctx_id_t context_handle,
                            gss_qop_t qop_req,
                            gss_iov_buffer_desc *iov,
                            int iov_count)
{
    OM_uint32 major_status;

    major_status = kg_seal_iov_length(minor_status, context_handle, FALSE,
                                      qop_req, NULL, iov, iov_count,
                                      KG_TOK_MIC_MSG);
    return major_status;
}
