/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/k5unsealiov.c */
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
#include "k5-der.h"
#include "gssapiP_krb5.h"

static OM_uint32
kg_unseal_v1_iov(krb5_context context,
                 OM_uint32 *minor_status,
                 krb5_gss_ctx_id_rec *ctx,
                 gss_iov_buffer_desc *iov,
                 int iov_count,
                 size_t token_wrapper_len,
                 int *conf_state,
                 gss_qop_t *qop_state,
                 int toktype)
{
    OM_uint32 code;
    gss_iov_buffer_t header;
    gss_iov_buffer_t trailer;
    unsigned char *ptr;
    int sealalg;
    int signalg;
    krb5_checksum md5cksum;
    size_t cksum_len = 0;
    size_t conflen = 0;
    int direction;
    krb5_ui_4 seqnum;
    OM_uint32 retval;
    size_t sumlen;
    krb5_keyusage sign_usage = KG_USAGE_SIGN;

    md5cksum.length = 0;
    md5cksum.contents = NULL;

    header = kg_locate_header_iov(iov, iov_count, toktype);
    assert(header != NULL);

    trailer = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);
    if (trailer != NULL && trailer->buffer.length != 0) {
        *minor_status = (OM_uint32)KRB5_BAD_MSIZE;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if (ctx->seq == NULL) {
        /* ctx was established using a newer enctype, and cannot process RFC
         * 1964 tokens. */
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if (header->buffer.length < token_wrapper_len + 22) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    ptr = (unsigned char *)header->buffer.value + token_wrapper_len;

    signalg  = ptr[0];
    signalg |= ptr[1] << 8;

    sealalg  = ptr[2];
    sealalg |= ptr[3] << 8;

    if (ptr[4] != 0xFF || ptr[5] != 0xFF) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if (toktype != KG_TOK_WRAP_MSG && sealalg != 0xFFFF) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if (toktype == KG_TOK_WRAP_MSG &&
        !(sealalg == 0xFFFF || sealalg == ctx->sealalg)) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if ((ctx->sealalg == SEAL_ALG_NONE && signalg > 1) ||
        (ctx->sealalg == SEAL_ALG_DES3KD &&
         signalg != SGN_ALG_HMAC_SHA1_DES3_KD)||
        (ctx->sealalg == SEAL_ALG_MICROSOFT_RC4 &&
         signalg != SGN_ALG_HMAC_MD5)) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    switch (signalg) {
    case SGN_ALG_HMAC_MD5:
        cksum_len = 8;
        if (toktype != KG_TOK_WRAP_MSG)
            sign_usage = 15;
        break;
    case SGN_ALG_HMAC_SHA1_DES3_KD:
        cksum_len = 20;
        break;
    default:
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    /* get the token parameters */
    code = kg_get_seq_num(context, ctx->seq, ptr + 14, ptr + 6, &direction,
                          &seqnum);
    if (code != 0) {
        *minor_status = code;
        return GSS_S_BAD_SIG;
    }

    /* decode the message, if SEAL */
    if (toktype == KG_TOK_WRAP_MSG) {
        if (sealalg != 0xFFFF) {
            if (ctx->sealalg == SEAL_ALG_MICROSOFT_RC4) {
                unsigned char bigend_seqnum[4];
                krb5_keyblock *enc_key;
                size_t i;

                store_32_be(seqnum, bigend_seqnum);

                code = krb5_k_key_keyblock(context, ctx->enc, &enc_key);
                if (code != 0) {
                    retval = GSS_S_FAILURE;
                    goto cleanup;
                }

                assert(enc_key->length == 16);

                for (i = 0; i < enc_key->length; i++)
                    ((char *)enc_key->contents)[i] ^= 0xF0;

                code = kg_arcfour_docrypt_iov(context, enc_key, 0,
                                              &bigend_seqnum[0], 4,
                                              iov, iov_count);
                krb5_free_keyblock(context, enc_key);
            } else {
                code = kg_decrypt_iov(context, 0,
                                      ((ctx->gss_flags & GSS_C_DCE_STYLE) != 0),
                                      0 /*EC*/, 0 /*RRC*/,
                                      ctx->enc, KG_USAGE_SEAL, NULL,
                                      iov, iov_count);
            }
            if (code != 0) {
                retval = GSS_S_FAILURE;
                goto cleanup;
            }
        }
        conflen = kg_confounder_size(context, ctx->enc->keyblock.enctype);
    }

    if (header->buffer.length != token_wrapper_len + 14 + cksum_len + conflen) {
        retval = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
    }

    /* compute the checksum of the message */

    /* initialize the checksum */

    switch (signalg) {
    case SGN_ALG_HMAC_MD5:
        md5cksum.checksum_type = CKSUMTYPE_HMAC_MD5_ARCFOUR;
        break;
    case SGN_ALG_HMAC_SHA1_DES3_KD:
        md5cksum.checksum_type = CKSUMTYPE_HMAC_SHA1_DES3;
        break;
    default:
        abort();
    }

    code = krb5_c_checksum_length(context, md5cksum.checksum_type, &sumlen);
    if (code != 0) {
        retval = GSS_S_FAILURE;
        goto cleanup;
    }
    md5cksum.length = sumlen;

    /* compute the checksum of the message */
    code = kg_make_checksum_iov_v1(context, md5cksum.checksum_type,
                                   cksum_len, ctx->seq, ctx->enc,
                                   sign_usage, iov, iov_count, toktype,
                                   &md5cksum);
    if (code != 0) {
        retval = GSS_S_FAILURE;
        goto cleanup;
    }

    switch (signalg) {
    case SGN_ALG_HMAC_SHA1_DES3_KD:
    case SGN_ALG_HMAC_MD5:
        code = k5_bcmp(md5cksum.contents, ptr + 14, cksum_len);
        break;
    default:
        code = 0;
        retval = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
        break;
    }

    if (code != 0) {
        code = 0;
        retval = GSS_S_BAD_SIG;
        goto cleanup;
    }

    /*
     * For GSS_C_DCE_STYLE, the caller manages the padding, because the
     * pad length is in the RPC PDU. The value of the padding may be
     * uninitialized. For normal GSS, the last bytes of the decrypted
     * data contain the pad length. kg_fixup_padding_iov() will find
     * this and fixup the last data IOV appropriately.
     */
    if (toktype == KG_TOK_WRAP_MSG &&
        (ctx->gss_flags & GSS_C_DCE_STYLE) == 0) {
        retval = kg_fixup_padding_iov(&code, iov, iov_count);
        if (retval != GSS_S_COMPLETE)
            goto cleanup;
    }

    if (conf_state != NULL)
        *conf_state = (sealalg != 0xFFFF);

    if (qop_state != NULL)
        *qop_state = GSS_C_QOP_DEFAULT;

    if ((ctx->initiate && direction != 0xff) ||
        (!ctx->initiate && direction != 0)) {
        *minor_status = (OM_uint32)G_BAD_DIRECTION;
        retval = GSS_S_BAD_SIG;
        goto cleanup;
    }

    code = 0;
    retval = g_seqstate_check(ctx->seqstate, (uint64_t)seqnum);

cleanup:
    krb5_free_checksum_contents(context, &md5cksum);

    *minor_status = code;

    return retval;
}

/* Similar to k5_der_get_value(), but output an unchecked content length
 * instead of a k5input containing the contents. */
static inline bool
get_der_tag(struct k5input *in, uint8_t idbyte, size_t *len_out)
{
    uint8_t lenbyte, i;
    size_t len;

    /* Do nothing if in is empty or the next byte doesn't match idbyte. */
    if (in->status || in->len == 0 || *in->ptr != idbyte)
        return false;

    /* Advance past the identifier byte and decode the length. */
    (void)k5_input_get_byte(in);
    lenbyte = k5_input_get_byte(in);
    if (lenbyte < 128) {
        len = lenbyte;
    } else {
        len = 0;
        for (i = 0; i < (lenbyte & 0x7F); i++) {
            if (len > (SIZE_MAX >> 8)) {
                k5_input_set_status(in, EOVERFLOW);
                return false;
            }
            len = (len << 8) | k5_input_get_byte(in);
        }
    }

    if (in->status)
        return false;

    *len_out = len;
    return true;
}

/*
 * Similar to g_verify_token_header() without toktype or flags, but do not read
 * more than *header_len bytes of ASN.1 wrapper, and on output set *header_len
 * to the remaining number of header bytes.  Verify the outer DER tag's length
 * against token_len, which may be larger (but not smaller) than *header_len.
 */
static gss_int32
verify_detached_wrapper(const gss_OID_desc *mech, size_t *header_len,
                        uint8_t **header_in, size_t token_len)
{
    struct k5input in, mech_der;
    gss_OID_desc toid;
    size_t len;

    k5_input_init(&in, *header_in, *header_len);

    if (get_der_tag(&in, 0x60, &len)) {
        if (len != token_len - (in.ptr - *header_in))
            return G_BAD_TOK_HEADER;
        if (!k5_der_get_value(&in, 0x06, &mech_der))
            return G_BAD_TOK_HEADER;
        toid.elements = (uint8_t *)mech_der.ptr;
        toid.length = mech_der.len;
        if (!g_OID_equal(&toid, mech))
            return G_WRONG_MECH;
    }

    *header_in = (uint8_t *)in.ptr;
    *header_len = in.len;
    return 0;
}

/*
 * Caller must provide TOKEN | DATA | PADDING | TRAILER, except
 * for DCE in which case it can just provide TOKEN | DATA (must
 * guarantee that DATA is padded)
 */
static OM_uint32
kg_unseal_iov_token(OM_uint32 *minor_status,
                    krb5_gss_ctx_id_rec *ctx,
                    int *conf_state,
                    gss_qop_t *qop_state,
                    gss_iov_buffer_desc *iov,
                    int iov_count,
                    int toktype)
{
    krb5_error_code code;
    krb5_context context = ctx->k5_context;
    unsigned char *ptr;
    gss_iov_buffer_t header;
    gss_iov_buffer_t padding;
    gss_iov_buffer_t trailer;
    size_t input_length, hlen;
    int toktype2;

    header = kg_locate_header_iov(iov, iov_count, toktype);
    if (header == NULL) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    padding = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_PADDING);
    trailer = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);

    ptr = (unsigned char *)header->buffer.value;
    input_length = header->buffer.length;

    if ((ctx->gss_flags & GSS_C_DCE_STYLE) == 0 &&
        toktype == KG_TOK_WRAP_MSG) {
        size_t data_length, assoc_data_length;

        kg_iov_msglen(iov, iov_count, &data_length, &assoc_data_length);

        input_length += data_length - assoc_data_length;

        if (padding != NULL)
            input_length += padding->buffer.length;

        if (trailer != NULL)
            input_length += trailer->buffer.length;
    }

    hlen = header->buffer.length;
    code = verify_detached_wrapper(ctx->mech_used, &hlen, &ptr, input_length);
    if (code != 0) {
        *minor_status = code;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if (hlen < 2) {
        *minor_status = (OM_uint32)G_BAD_TOK_HEADER;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    toktype2 = load_16_be(ptr);

    ptr += 2;
    hlen -= 2;

    switch (toktype2) {
    case KG2_TOK_MIC_MSG:
    case KG2_TOK_WRAP_MSG:
    case KG2_TOK_DEL_CTX:
        code = gss_krb5int_unseal_v3_iov(context, minor_status, ctx, iov, iov_count,
                                         conf_state, qop_state, toktype);
        break;
    case KG_TOK_MIC_MSG:
    case KG_TOK_WRAP_MSG:
    case KG_TOK_DEL_CTX:
        code = kg_unseal_v1_iov(context, minor_status, ctx, iov, iov_count,
                                (size_t)(ptr - (unsigned char *)header->buffer.value),
                                conf_state, qop_state, toktype);
        break;
    default:
        *minor_status = (OM_uint32)G_BAD_TOK_HEADER;
        code = GSS_S_DEFECTIVE_TOKEN;
        break;
    }

    if (code != 0)
        save_error_info(*minor_status, context);

    return code;
}

/*
 * Split a STREAM | SIGN_DATA | DATA into
 *         HEADER | SIGN_DATA | DATA | PADDING | TRAILER
 */
static OM_uint32
kg_unseal_stream_iov(OM_uint32 *minor_status,
                     krb5_gss_ctx_id_rec *ctx,
                     int *conf_state,
                     gss_qop_t *qop_state,
                     gss_iov_buffer_desc *iov,
                     int iov_count,
                     int toktype)
{
    unsigned char *ptr;
    unsigned int bodysize;
    OM_uint32 code = 0, major_status = GSS_S_FAILURE;
    krb5_context context = ctx->k5_context;
    int conf_req_flag, toktype2;
    int i = 0, j;
    gss_iov_buffer_desc *tiov = NULL;
    gss_iov_buffer_t stream, data = NULL;
    gss_iov_buffer_t theader, tdata = NULL, tpadding, ttrailer;

    assert(toktype == KG_TOK_WRAP_MSG);

    if (toktype != KG_TOK_WRAP_MSG || (ctx->gss_flags & GSS_C_DCE_STYLE)) {
        code = EINVAL;
        goto cleanup;
    }

    stream = kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_STREAM);
    assert(stream != NULL);

    ptr = (unsigned char *)stream->buffer.value;

    code = g_verify_token_header(ctx->mech_used,
                                 &bodysize, &ptr, -1,
                                 stream->buffer.length, 0);
    if (code != 0) {
        major_status = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
    }

    if (bodysize < 2) {
        *minor_status = (OM_uint32)G_BAD_TOK_HEADER;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    toktype2 = load_16_be(ptr);

    ptr += 2;
    bodysize -= 2;

    tiov = (gss_iov_buffer_desc *)calloc((size_t)iov_count + 2, sizeof(gss_iov_buffer_desc));
    if (tiov == NULL) {
        code = ENOMEM;
        goto cleanup;
    }

    /* HEADER */
    theader = &tiov[i++];
    theader->type = GSS_IOV_BUFFER_TYPE_HEADER;
    theader->buffer.value = stream->buffer.value;
    theader->buffer.length = ptr - (unsigned char *)stream->buffer.value;
    if (bodysize < 14 ||
        stream->buffer.length != theader->buffer.length + bodysize) {
        major_status = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
    }
    theader->buffer.length += 14;

    /* n[SIGN_DATA] | DATA | m[SIGN_DATA] */
    for (j = 0; j < iov_count; j++) {
        OM_uint32 type = GSS_IOV_BUFFER_TYPE(iov[j].type);

        if (type == GSS_IOV_BUFFER_TYPE_DATA) {
            if (data != NULL) {
                /* only a single DATA buffer can appear */
                code = EINVAL;
                goto cleanup;
            }

            data = &iov[j];
            tdata = &tiov[i];
        }
        if (type == GSS_IOV_BUFFER_TYPE_DATA ||
            type == GSS_IOV_BUFFER_TYPE_SIGN_ONLY)
            tiov[i++] = iov[j];
    }

    if (data == NULL) {
        /* a single DATA buffer must be present */
        code = EINVAL;
        goto cleanup;
    }

    /* PADDING | TRAILER */
    tpadding = &tiov[i++];
    tpadding->type = GSS_IOV_BUFFER_TYPE_PADDING;
    tpadding->buffer.length = 0;
    tpadding->buffer.value = NULL;

    ttrailer = &tiov[i++];
    ttrailer->type = GSS_IOV_BUFFER_TYPE_TRAILER;

    switch (toktype2) {
    case KG2_TOK_MIC_MSG:
    case KG2_TOK_WRAP_MSG:
    case KG2_TOK_DEL_CTX: {
        size_t ec, rrc;
        krb5_enctype enctype;
        unsigned int k5_headerlen = 0;
        unsigned int k5_trailerlen = 0;

        if (ctx->have_acceptor_subkey)
            enctype = ctx->acceptor_subkey->keyblock.enctype;
        else
            enctype = ctx->subkey->keyblock.enctype;
        conf_req_flag = ((ptr[0] & FLAG_WRAP_CONFIDENTIAL) != 0);
        ec = conf_req_flag ? load_16_be(ptr + 2) : 0;
        rrc = load_16_be(ptr + 4);

        if (rrc != 0) {
            if (!gss_krb5int_rotate_left((unsigned char *)stream->buffer.value + 16,
                                         stream->buffer.length - 16, rrc)) {
                code = ENOMEM;
                goto cleanup;
            }
            store_16_be(0, ptr + 4); /* set RRC to zero */
        }

        if (conf_req_flag) {
            code = krb5_c_crypto_length(context, enctype, KRB5_CRYPTO_TYPE_HEADER, &k5_headerlen);
            if (code != 0)
                goto cleanup;
            theader->buffer.length += k5_headerlen; /* length validated later */
        }

        /* no PADDING for CFX, EC is used instead */
        code = krb5_c_crypto_length(context, enctype,
                                    conf_req_flag ? KRB5_CRYPTO_TYPE_TRAILER : KRB5_CRYPTO_TYPE_CHECKSUM,
                                    &k5_trailerlen);
        if (code != 0)
            goto cleanup;

        ttrailer->buffer.length = ec + (conf_req_flag ? 16 : 0 /* E(Header) */) + k5_trailerlen;
        ttrailer->buffer.value = (unsigned char *)stream->buffer.value +
            stream->buffer.length - ttrailer->buffer.length;
        break;
    }
    case KG_TOK_MIC_MSG:
    case KG_TOK_WRAP_MSG:
    case KG_TOK_DEL_CTX:
        theader->buffer.length += ctx->cksum_size +
            kg_confounder_size(context, ctx->enc->keyblock.enctype);

        /*
         * we can't set the padding accurately until decryption;
         * kg_fixup_padding_iov() will take care of this
         */
        tpadding->buffer.length = 1;
        tpadding->buffer.value = (unsigned char *)stream->buffer.value + stream->buffer.length - 1;

        /* no TRAILER for pre-CFX */
        ttrailer->buffer.length = 0;
        ttrailer->buffer.value = NULL;

        break;
    default:
        code = (OM_uint32)G_BAD_TOK_HEADER;
        major_status = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
        break;
    }

    /* IOV: -----------0-------------+---1---+--2--+----------------3--------------*/
    /* Old: GSS-Header | Conf        | Data  | Pad |                               */
    /* CFX: GSS-Header | Kerb-Header | Data  |     | EC | E(Header) | Kerb-Trailer */
    /* GSS: -------GSS-HEADER--------+-DATA--+-PAD-+----------GSS-TRAILER----------*/

    /* validate lengths */
    if (stream->buffer.length < theader->buffer.length +
        tpadding->buffer.length +
        ttrailer->buffer.length)
    {
        code = (OM_uint32)KRB5_BAD_MSIZE;
        major_status = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
    }

    /* setup data */
    tdata->buffer.length = stream->buffer.length - ttrailer->buffer.length -
        tpadding->buffer.length - theader->buffer.length;

    assert(data != NULL);

    if (data->type & GSS_IOV_BUFFER_FLAG_ALLOCATE) {
        code = kg_allocate_iov(tdata, tdata->buffer.length);
        if (code != 0)
            goto cleanup;
        memcpy(tdata->buffer.value,
               (unsigned char *)stream->buffer.value + theader->buffer.length, tdata->buffer.length);
    } else
        tdata->buffer.value = (unsigned char *)stream->buffer.value + theader->buffer.length;

    assert(i <= iov_count + 2);

    major_status = kg_unseal_iov_token(&code, ctx, conf_state, qop_state,
                                       tiov, i, toktype);
    if (major_status == GSS_S_COMPLETE)
        *data = *tdata;
    else
        kg_release_iov(tdata, 1);

cleanup:
    if (tiov != NULL)
        free(tiov);

    *minor_status = code;

    return major_status;
}

OM_uint32
kg_unseal_iov(OM_uint32 *minor_status,
              gss_ctx_id_t context_handle,
              int *conf_state,
              gss_qop_t *qop_state,
              gss_iov_buffer_desc *iov,
              int iov_count,
              int toktype)
{
    krb5_gss_ctx_id_rec *ctx;
    OM_uint32 code;

    ctx = (krb5_gss_ctx_id_rec *)context_handle;
    if (ctx->terminated || !ctx->established) {
        *minor_status = KG_CTX_INCOMPLETE;
        return GSS_S_NO_CONTEXT;
    }

    if (kg_locate_iov(iov, iov_count, GSS_IOV_BUFFER_TYPE_STREAM) != NULL) {
        code = kg_unseal_stream_iov(minor_status, ctx, conf_state, qop_state,
                                    iov, iov_count, toktype);
    } else {
        code = kg_unseal_iov_token(minor_status, ctx, conf_state, qop_state,
                                   iov, iov_count, toktype);
    }

    return code;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_unwrap_iov(OM_uint32 *minor_status,
                    gss_ctx_id_t context_handle,
                    int *conf_state,
                    gss_qop_t *qop_state,
                    gss_iov_buffer_desc *iov,
                    int iov_count)
{
    OM_uint32 major_status;

    major_status = kg_unseal_iov(minor_status, context_handle,
                                 conf_state, qop_state,
                                 iov, iov_count, KG_TOK_WRAP_MSG);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_verify_mic_iov(OM_uint32 *minor_status,
                        gss_ctx_id_t context_handle,
                        gss_qop_t *qop_state,
                        gss_iov_buffer_desc *iov,
                        int iov_count)
{
    OM_uint32 major_status;

    major_status = kg_unseal_iov(minor_status, context_handle,
                                 NULL, qop_state,
                                 iov, iov_count, KG_TOK_MIC_MSG);

    return major_status;
}
