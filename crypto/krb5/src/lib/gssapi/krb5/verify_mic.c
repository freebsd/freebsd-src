/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/verify_mic.c - krb5 gss_verify_mic() implementation */
/*
 * Copyright (C) 2024 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gssapiP_krb5.h"

OM_uint32
kg_verify_mic_v1(krb5_context context, OM_uint32 *minor_status,
                 krb5_gss_ctx_id_rec *ctx, uint16_t exp_toktype,
                 struct k5input *in, gss_buffer_t message)
{
    krb5_error_code ret = 0;
    krb5_keyusage usage;
    const uint8_t *header, *seqbytes, *cksum;
    int direction;
    size_t cksum_len;
    uint32_t seqnum, filler;
    uint16_t toktype, signalg;

    if (ctx->seq == NULL) {
        /* ctx was established using a newer enctype, and cannot process RFC
         * 1964 tokens. */
        return GSS_S_DEFECTIVE_TOKEN;
    }

    header = in->ptr;
    toktype = k5_input_get_uint16_be(in);
    signalg = k5_input_get_uint16_le(in);
    filler = k5_input_get_uint32_le(in);
    seqbytes = k5_input_get_bytes(in, 8);
    cksum_len = (signalg == SGN_ALG_HMAC_SHA1_DES3_KD) ? 20 : 8;
    cksum = k5_input_get_bytes(in, cksum_len);

    if (in->status || in->len != 0 || toktype != exp_toktype ||
        filler != 0xFFFFFFFF || signalg != ctx->signalg)
        return GSS_S_DEFECTIVE_TOKEN;
    usage = (signalg == SGN_ALG_HMAC_MD5) ? 15 : KG_USAGE_SIGN;

    ret = kg_get_seq_num(context, ctx->seq, cksum, seqbytes, &direction,
                         &seqnum);
    if (ret) {
        *minor_status = ret;
        return GSS_S_BAD_SIG;
    }

    if (!kg_verify_checksum_v1(context, signalg, ctx->seq, usage, header,
                               message->value, message->length,
                               cksum, cksum_len))
        return GSS_S_BAD_SIG;

    if ((ctx->initiate && direction != 0xff) ||
        (!ctx->initiate && direction != 0)) {
        *minor_status = (OM_uint32)G_BAD_DIRECTION;
        return GSS_S_BAD_SIG;
    }

    return g_seqstate_check(ctx->seqstate, seqnum);
}

static OM_uint32
verify_mic_v3(krb5_context context, OM_uint32 *minor_status,
              krb5_gss_ctx_id_rec *ctx, struct k5input *in,
              gss_buffer_t message)
{
    krb5_keyusage usage;
    krb5_key key;
    krb5_cksumtype cksumtype;
    uint64_t seqnum;
    uint32_t filler2;
    uint16_t toktype;
    uint8_t flags, filler1;

    toktype = k5_input_get_uint16_be(in);
    flags = k5_input_get_byte(in);
    filler1 = k5_input_get_byte(in);
    filler2 = k5_input_get_uint32_be(in);
    seqnum = k5_input_get_uint64_be(in);

    if (in->status || toktype != KG2_TOK_MIC_MSG || filler1 != 0xFF ||
        filler2 != 0xFFFFFFFF)
        return GSS_S_DEFECTIVE_TOKEN;

    if (!!(flags & FLAG_SENDER_IS_ACCEPTOR) != ctx->initiate) {
        *minor_status = (OM_uint32)G_BAD_DIRECTION;
        return GSS_S_BAD_SIG;
    }

    usage = ctx->initiate ? KG_USAGE_ACCEPTOR_SIGN : KG_USAGE_INITIATOR_SIGN;
    if (ctx->have_acceptor_subkey && (flags & FLAG_ACCEPTOR_SUBKEY)) {
        key = ctx->acceptor_subkey;
        cksumtype = ctx->acceptor_subkey_cksumtype;
    } else {
        key = ctx->subkey;
        cksumtype = ctx->cksumtype;
    }
    assert(key != NULL);

    if (!kg_verify_checksum_v3(context, key, usage, cksumtype, KG2_TOK_MIC_MSG,
                               flags, seqnum, message->value, message->length,
                               in->ptr, in->len))
        return GSS_S_BAD_SIG;

    return g_seqstate_check(ctx->seqstate, seqnum);
}

OM_uint32 KRB5_CALLCONV
krb5_gss_verify_mic(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                    gss_buffer_t message, gss_buffer_t token,
                    gss_qop_t *qop_state)
{
    krb5_gss_ctx_id_rec *ctx = (krb5_gss_ctx_id_rec *)context_handle;
    uint16_t toktype;
    OM_uint32 major;
    struct k5input in, unwrapped;

    *minor_status = 0;
    if (qop_state != NULL)
        *qop_state = GSS_C_QOP_DEFAULT;

    if (ctx->terminated || !ctx->established) {
        *minor_status = KG_CTX_INCOMPLETE;
        return GSS_S_NO_CONTEXT;
    }

    k5_input_init(&in, token->value, token->length);
    (void)g_verify_token_header(&in, ctx->mech_used);
    unwrapped = in;

    toktype = k5_input_get_uint16_be(&in);
    if (toktype == KG_TOK_MIC_MSG) {
        major = kg_verify_mic_v1(ctx->k5_context, minor_status, ctx, toktype,
                                 &unwrapped, message);
    } else if (toktype == KG2_TOK_MIC_MSG) {
        major = verify_mic_v3(ctx->k5_context, minor_status, ctx, &unwrapped,
                              message);
    } else {
        *minor_status = (OM_uint32)G_BAD_TOK_HEADER;
        major = GSS_S_DEFECTIVE_TOKEN;
    }

    if (major)
        save_error_info(*minor_status, ctx->k5_context);

    return major;
}
