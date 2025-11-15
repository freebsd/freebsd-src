/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/unwrap.c - krb5 gss_unwrap() implementation */
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

/* The RFC 1964 token format is only used with DES3 and RC4, both of which use
 * an 8-byte confounder. */
#define V1_CONFOUNDER_LEN 8

#define V3_HEADER_LEN 16

/* Perform raw decryption (unauthenticated, no change in length) from in into
 * *out.  For RC4, use seqnum to derive the encryption key. */
static krb5_error_code
decrypt_v1(krb5_context context, uint16_t sealalg, krb5_key key,
           uint32_t seqnum, const uint8_t *in, size_t len, uint8_t **out)
{
    krb5_error_code ret;
    uint8_t bigend_seqnum[4], *plain = NULL;
    krb5_keyblock *enc_key = NULL;
    unsigned int i;

    *out = NULL;

    plain = malloc(len);
    if (plain == NULL)
        return ENOMEM;

    if (sealalg != SEAL_ALG_MICROSOFT_RC4) {
        ret = kg_decrypt(context, key, KG_USAGE_SEAL, NULL, in, plain, len);
        if (ret)
            goto cleanup;
    } else {
        store_32_be(seqnum, bigend_seqnum);
        ret = krb5_k_key_keyblock(context, key, &enc_key);
        if (ret)
            goto cleanup;
        for (i = 0; i < enc_key->length; i++)
            enc_key->contents[i] ^= 0xF0;
        ret = kg_arcfour_docrypt(enc_key, 0, bigend_seqnum, 4, in, len, plain);
        if (ret)
            goto cleanup;
    }

    *out = plain;
    plain = NULL;

cleanup:
    free(plain);
    krb5_free_keyblock(context, enc_key);
    return ret;
}

static OM_uint32
unwrap_v1(krb5_context context, OM_uint32 *minor_status,
          krb5_gss_ctx_id_rec *ctx, struct k5input *in,
          gss_buffer_t output_message, int *conf_state)
{
    krb5_error_code ret = 0;
    OM_uint32 major;
    uint8_t *decrypted = NULL;
    const uint8_t *plain, *header, *seqbytes, *cksum;
    int direction, bad_pad = 0;
    size_t plainlen, cksum_len;
    uint32_t seqnum;
    uint16_t toktype, signalg, sealalg, filler;
    uint8_t padlen;

    if (ctx->seq == NULL) {
        /* ctx was established using a newer enctype, and cannot process RFC
         * 1964 tokens. */
        major = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
    }

    /* Parse the header fields and fetch the checksum. */
    header = in->ptr;
    toktype = k5_input_get_uint16_be(in);
    signalg = k5_input_get_uint16_le(in);
    sealalg = k5_input_get_uint16_le(in);
    filler = k5_input_get_uint16_le(in);
    seqbytes = k5_input_get_bytes(in, 8);
    cksum_len = (signalg == SGN_ALG_HMAC_SHA1_DES3_KD) ? 20 : 8;
    cksum = k5_input_get_bytes(in, cksum_len);

    /* Validate the header fields, and ensure that there are enough bytes
     * remaining for a confounder and padding length byte. */
    if (in->status || in->len < V1_CONFOUNDER_LEN + 1 ||
        toktype != KG_TOK_WRAP_MSG || filler != 0xFFFF ||
        signalg != ctx->signalg ||
        (sealalg != SEAL_ALG_NONE && sealalg != ctx->sealalg)) {
        major = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
    }

    ret = kg_get_seq_num(context, ctx->seq, cksum, seqbytes, &direction,
                         &seqnum);
    if (ret) {
        major = GSS_S_BAD_SIG;
        goto cleanup;
    }

    /* Decrypt the ciphertext, or just accept the remaining bytes as the
     * plaintext (still with a confounder and padding length byte). */
    plain = in->ptr;
    plainlen = in->len;
    if (sealalg != SEAL_ALG_NONE) {
        ret = decrypt_v1(context, sealalg, ctx->enc, seqnum, in->ptr, in->len,
                         &decrypted);
        if (ret) {
            major = GSS_S_FAILURE;
            goto cleanup;
        }
        plain = decrypted;
    }

    padlen = plain[plainlen - 1];
    if (plainlen - V1_CONFOUNDER_LEN < padlen) {
        /* Don't error out yet, to avoid padding oracle attacks.  We will
         * treat this as a checksum failure later on. */
        padlen = 0;
        bad_pad = 1;
    }

    if (!kg_verify_checksum_v1(context, signalg, ctx->seq, KG_USAGE_SIGN,
                               header, plain, plainlen, cksum, cksum_len) ||
        bad_pad) {
        major = GSS_S_BAD_SIG;
        goto cleanup;
    }

    if ((ctx->initiate && direction != 0xff) ||
        (!ctx->initiate && direction != 0)) {
        *minor_status = (OM_uint32)G_BAD_DIRECTION;
        major = GSS_S_BAD_SIG;
        goto cleanup;
    }

    output_message->length = plainlen - V1_CONFOUNDER_LEN - padlen;
    if (output_message->length > 0) {
        output_message->value = gssalloc_malloc(output_message->length);
        if (output_message->value == NULL) {
            ret = ENOMEM;
            major = GSS_S_FAILURE;
            goto cleanup;
        }
        memcpy(output_message->value, plain + V1_CONFOUNDER_LEN,
               output_message->length);
    }

    if (conf_state != NULL)
        *conf_state = (sealalg != SEAL_ALG_NONE);

    major = g_seqstate_check(ctx->seqstate, seqnum);

cleanup:
    free(decrypted);
    *minor_status = ret;
    return major;
}

/* Return true if plain ends with an RFC 4121 header with the provided fields,
 * and that plain contains at least ec additional bytes of padding. */
static krb5_boolean
verify_enc_header(krb5_data *plain, uint8_t flags, size_t ec, uint64_t seqnum)
{
    uint8_t *h;

    if (plain->length < V3_HEADER_LEN + ec)
        return FALSE;
    h = (uint8_t *)plain->data + plain->length - V3_HEADER_LEN;
    return load_16_be(h) == KG2_TOK_WRAP_MSG && h[2] == flags &&
        h[3] == 0xFF && load_16_be(h + 4) == ec &&
        load_64_be(h + 8) == seqnum;
}

/* Decrypt ctext, verify the encrypted header, and return the appropriately
 * truncated plaintext in out, allocated with gssalloc_malloc(). */
static OM_uint32
decrypt_v3(krb5_context context, OM_uint32 *minor_status,
           krb5_key key, krb5_keyusage usage, const uint8_t *ctext, size_t len,
           uint8_t flags, size_t ec, uint64_t seqnum, gss_buffer_t out)
{
    OM_uint32 major;
    krb5_error_code ret;
    krb5_enc_data cipher;
    krb5_data plain;
    uint8_t *buf = NULL;

    buf = gssalloc_malloc(len);
    if (buf == NULL) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }

    cipher.enctype = key->keyblock.enctype;
    cipher.ciphertext = make_data((uint8_t *)ctext, len);
    plain = make_data(buf, len);
    ret = krb5_k_decrypt(context, key, usage, NULL, &cipher, &plain);
    if (ret) {
        *minor_status = ret;
        major = GSS_S_BAD_SIG;
        goto cleanup;
    }

    if (!verify_enc_header(&plain, flags, ec, seqnum)) {
        major = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
    }

    out->length = plain.length - ec - 16;
    out->value = buf;
    buf = NULL;
    if (out->length == 0) {
        gssalloc_free(out->value);
        out->value = NULL;
    }

    major = GSS_S_COMPLETE;

cleanup:
    gssalloc_free(buf);
    return major;
}

/* Place a rotated copy of data in *storage and return it, or return data if no
 * rotation is required.  Return null on allocation failure. */
static const uint8_t *
rotate_left(const uint8_t *data, size_t len, size_t rc, uint8_t **storage)
{
    if (len == 0 || rc % len == 0)
        return data;
    rc %= len;

    *storage = malloc(len);
    if (*storage == NULL)
        return NULL;
    memcpy(*storage, data + rc, len - rc);
    memcpy(*storage + len - rc, data, rc);
    return *storage;
}

static OM_uint32
unwrap_v3(krb5_context context, OM_uint32 *minor_status,
          krb5_gss_ctx_id_rec *ctx, struct k5input *in,
          gss_buffer_t output_message, int *conf_state)
{
    OM_uint32 major;
    krb5_error_code ret = 0;
    krb5_keyusage usage;
    krb5_key key;
    krb5_cksumtype cksumtype;
    size_t ec, rrc, cksumsize, plen, data_len;
    uint64_t seqnum;
    uint16_t toktype;
    uint8_t flags, filler, *rotated = NULL;
    const uint8_t *payload;

    toktype = k5_input_get_uint16_be(in);
    flags = k5_input_get_byte(in);
    filler = k5_input_get_byte(in);
    ec = k5_input_get_uint16_be(in);
    rrc = k5_input_get_uint16_be(in);
    seqnum = k5_input_get_uint64_be(in);

    if (in->status || toktype != KG2_TOK_WRAP_MSG || filler != 0xFF) {
        major = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
    }

    if (!!(flags & FLAG_SENDER_IS_ACCEPTOR) != ctx->initiate) {
        major = GSS_S_BAD_SIG;
        *minor_status = (OM_uint32)G_BAD_DIRECTION;
        goto cleanup;
    }

    usage = ctx->initiate ? KG_USAGE_ACCEPTOR_SEAL : KG_USAGE_INITIATOR_SEAL;
    if (ctx->have_acceptor_subkey && (flags & FLAG_ACCEPTOR_SUBKEY)) {
        key = ctx->acceptor_subkey;
        cksumtype = ctx->acceptor_subkey_cksumtype;
    } else {
        key = ctx->subkey;
        cksumtype = ctx->cksumtype;
    }
    assert(key != NULL);

    payload = rotate_left(in->ptr, in->len, rrc, &rotated);
    plen = in->len;
    if (payload == NULL) {
        major = GSS_S_FAILURE;
        *minor_status = ENOMEM;
        goto cleanup;
    }
    if (flags & FLAG_WRAP_CONFIDENTIAL) {
        major = decrypt_v3(context, minor_status, key, usage, payload, plen,
                           flags, ec, seqnum, output_message);
        if (major != GSS_S_COMPLETE)
            goto cleanup;
    } else {
        /* Divide the payload into data and checksum. */
        ret = krb5_c_checksum_length(context, cksumtype, &cksumsize);
        if (ret) {
            major = GSS_S_FAILURE;
            *minor_status = ret;
            goto cleanup;
        }
        if (cksumsize > plen || ec != cksumsize) {
            major = GSS_S_DEFECTIVE_TOKEN;
            goto cleanup;
        }
        data_len = plen - cksumsize;

        if (!kg_verify_checksum_v3(context, key, usage, cksumtype,
                                   KG2_TOK_WRAP_MSG, flags, seqnum,
                                   payload, data_len,
                                   payload + data_len, cksumsize)) {
            major = GSS_S_BAD_SIG;
            goto cleanup;
        }

        output_message->length = data_len;
        if (data_len > 0) {
            output_message->value = gssalloc_malloc(data_len);
            if (output_message->value == NULL) {
                major = GSS_S_FAILURE;
                *minor_status = ENOMEM;
                goto cleanup;
            }
            memcpy(output_message->value, payload, data_len);
        }
        output_message->length = data_len;
    }

    if (conf_state != NULL)
        *conf_state = !!(flags & FLAG_WRAP_CONFIDENTIAL);

    major = g_seqstate_check(ctx->seqstate, seqnum);

cleanup:
    free(rotated);
    return major;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_unwrap(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
                gss_buffer_t input_message, gss_buffer_t output_message,
                int *conf_state, gss_qop_t *qop_state)
{
    krb5_gss_ctx_id_rec *ctx = (krb5_gss_ctx_id_rec *)context_handle;
    uint16_t toktype;
    OM_uint32 major;
    struct k5input in, unwrapped;

    *minor_status = 0;
    output_message->value = NULL;
    output_message->length = 0;
    if (qop_state != NULL)
        *qop_state = GSS_C_QOP_DEFAULT;

    if (ctx->terminated || !ctx->established) {
        *minor_status = KG_CTX_INCOMPLETE;
        return GSS_S_NO_CONTEXT;
    }

    k5_input_init(&in, input_message->value, input_message->length);
    (void)g_verify_token_header(&in, ctx->mech_used);
    unwrapped = in;

    toktype = k5_input_get_uint16_be(&in);
    if (toktype == KG_TOK_WRAP_MSG) {
        major = unwrap_v1(ctx->k5_context, minor_status, ctx, &unwrapped,
                          output_message, conf_state);
    } else if (toktype == KG2_TOK_WRAP_MSG) {
        major = unwrap_v3(ctx->k5_context, minor_status, ctx, &unwrapped,
                          output_message, conf_state);
    } else {
        *minor_status = (OM_uint32)G_BAD_TOK_HEADER;
        major = GSS_S_DEFECTIVE_TOKEN;
    }

    if (major)
        save_error_info(*minor_status, ctx->k5_context);

    return major;
}
