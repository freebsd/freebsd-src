/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2001, 2007 by the Massachusetts Institute of Technology.
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "gssapiP_krb5.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <assert.h>

/* message_buffer is an input if SIGN, output if SEAL, and ignored if DEL_CTX
   conf_state is only valid if SEAL. */

static OM_uint32
kg_unseal_v1(context, minor_status, ctx, ptr, bodysize, message_buffer,
             conf_state, qop_state, toktype)
    krb5_context context;
    OM_uint32 *minor_status;
    krb5_gss_ctx_id_rec *ctx;
    unsigned char *ptr;
    int bodysize;
    gss_buffer_t message_buffer;
    int *conf_state;
    gss_qop_t *qop_state;
    int toktype;
{
    krb5_error_code code;
    int conflen = 0;
    int signalg;
    int sealalg;
    int bad_pad = 0;
    gss_buffer_desc token;
    krb5_checksum cksum;
    krb5_checksum md5cksum;
    krb5_data plaind;
    char *data_ptr;
    unsigned char *plain;
    unsigned int cksum_len = 0;
    size_t plainlen;
    int direction;
    krb5_ui_4 seqnum;
    OM_uint32 retval;
    size_t sumlen;
    size_t padlen;
    krb5_keyusage sign_usage = KG_USAGE_SIGN;

    if (toktype == KG_TOK_SEAL_MSG) {
        message_buffer->length = 0;
        message_buffer->value = NULL;
    }

    /* Sanity checks */

    if (ctx->seq == NULL) {
        /* ctx was established using a newer enctype, and cannot process RFC
         * 1964 tokens. */
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if ((bodysize < 22) || (ptr[4] != 0xff) || (ptr[5] != 0xff)) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    signalg = ptr[0] + (ptr[1]<<8);
    sealalg = ptr[2] + (ptr[3]<<8);

    if ((toktype != KG_TOK_SEAL_MSG) &&
        (sealalg != 0xffff)) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    /* in the current spec, there is only one valid seal algorithm per
       key type, so a simple comparison is ok */

    if ((toktype == KG_TOK_SEAL_MSG) &&
        !((sealalg == 0xffff) ||
          (sealalg == ctx->sealalg))) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    /* there are several mappings of seal algorithms to sign algorithms,
       but few enough that we can try them all. */

    if ((ctx->sealalg == SEAL_ALG_NONE && signalg > 1) ||
        (ctx->sealalg == SEAL_ALG_1 && signalg != SGN_ALG_3) ||
        (ctx->sealalg == SEAL_ALG_DES3KD &&
         signalg != SGN_ALG_HMAC_SHA1_DES3_KD)||
        (ctx->sealalg == SEAL_ALG_MICROSOFT_RC4 &&
         signalg != SGN_ALG_HMAC_MD5)) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    switch (signalg) {
    case SGN_ALG_DES_MAC_MD5:
    case SGN_ALG_MD2_5:
    case SGN_ALG_HMAC_MD5:
        cksum_len = 8;
        if (toktype != KG_TOK_SEAL_MSG)
            sign_usage = 15;
        break;
    case SGN_ALG_3:
        cksum_len = 16;
        break;
    case SGN_ALG_HMAC_SHA1_DES3_KD:
        cksum_len = 20;
        break;
    default:
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if ((size_t)bodysize < 14 + cksum_len) {
        *minor_status = 0;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    /* get the token parameters */

    if ((code = kg_get_seq_num(context, ctx->seq, ptr+14, ptr+6, &direction,
                               &seqnum))) {
        *minor_status = code;
        return(GSS_S_BAD_SIG);
    }

    /* decode the message, if SEAL */

    if (toktype == KG_TOK_SEAL_MSG) {
        size_t tmsglen = bodysize-(14+cksum_len);
        if (sealalg != 0xffff) {
            if ((plain = (unsigned char *) xmalloc(tmsglen)) == NULL) {
                *minor_status = ENOMEM;
                return(GSS_S_FAILURE);
            }
            if (ctx->sealalg == SEAL_ALG_MICROSOFT_RC4) {
                unsigned char bigend_seqnum[4];
                krb5_keyblock *enc_key;
                int i;
                store_32_be(seqnum, bigend_seqnum);
                code = krb5_k_key_keyblock(context, ctx->enc, &enc_key);
                if (code)
                {
                    xfree(plain);
                    *minor_status = code;
                    return(GSS_S_FAILURE);
                }

                assert (enc_key->length == 16);
                for (i = 0; i <= 15; i++)
                    ((char *) enc_key->contents)[i] ^=0xf0;
                code = kg_arcfour_docrypt (enc_key, 0,
                                           &bigend_seqnum[0], 4,
                                           ptr+14+cksum_len, tmsglen,
                                           plain);
                krb5_free_keyblock (context, enc_key);
            } else {
                code = kg_decrypt(context, ctx->enc, KG_USAGE_SEAL, NULL,
                                  ptr+14+cksum_len, plain, tmsglen);
            }
            if (code) {
                xfree(plain);
                *minor_status = code;
                return(GSS_S_FAILURE);
            }
        } else {
            plain = ptr+14+cksum_len;
        }

        plainlen = tmsglen;

        conflen = kg_confounder_size(context, ctx->enc->keyblock.enctype);
        if (tmsglen < (size_t)conflen) {
            if (sealalg != 0xffff)
                xfree(plain);
            *minor_status = 0;
            return(GSS_S_DEFECTIVE_TOKEN);
        }
        padlen = plain[tmsglen - 1];
        if (tmsglen - conflen < padlen) {
            /* Don't error out yet, to avoid padding oracle attacks.  We will
             * treat this as a checksum failure later on. */
            padlen = 0;
            bad_pad = 1;
        }
        token.length = tmsglen - conflen - padlen;

        if (token.length) {
            if ((token.value = (void *) gssalloc_malloc(token.length)) == NULL) {
                if (sealalg != 0xffff)
                    xfree(plain);
                *minor_status = ENOMEM;
                return(GSS_S_FAILURE);
            }
            memcpy(token.value, plain+conflen, token.length);
        } else {
            token.value = NULL;
        }
    } else if (toktype == KG_TOK_SIGN_MSG) {
        token = *message_buffer;
        plain = token.value;
        plainlen = token.length;
    } else {
        token.length = 0;
        token.value = NULL;
        plain = token.value;
        plainlen = token.length;
    }

    /* compute the checksum of the message */

    /* initialize the the cksum */
    switch (signalg) {
    case SGN_ALG_DES_MAC_MD5:
    case SGN_ALG_MD2_5:
    case SGN_ALG_DES_MAC:
    case SGN_ALG_3:
        md5cksum.checksum_type = CKSUMTYPE_RSA_MD5;
        break;
    case SGN_ALG_HMAC_MD5:
        md5cksum.checksum_type = CKSUMTYPE_HMAC_MD5_ARCFOUR;
        break;
    case SGN_ALG_HMAC_SHA1_DES3_KD:
        md5cksum.checksum_type = CKSUMTYPE_HMAC_SHA1_DES3;
        break;
    default:
        abort ();
    }

    code = krb5_c_checksum_length(context, md5cksum.checksum_type, &sumlen);
    if (code)
        return(code);
    md5cksum.length = sumlen;

    switch (signalg) {
    case SGN_ALG_DES_MAC_MD5:
    case SGN_ALG_3:
        /* compute the checksum of the message */

        /* 8 = bytes of token body to be checksummed according to spec */

        if (! (data_ptr = xmalloc(8 + plainlen))) {
            if (sealalg != 0xffff)
                xfree(plain);
            if (toktype == KG_TOK_SEAL_MSG)
                gssalloc_free(token.value);
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }

        (void) memcpy(data_ptr, ptr-2, 8);

        (void) memcpy(data_ptr+8, plain, plainlen);

        plaind.length = 8 + plainlen;
        plaind.data = data_ptr;
        code = krb5_k_make_checksum(context, md5cksum.checksum_type,
                                    ctx->seq, sign_usage,
                                    &plaind, &md5cksum);
        xfree(data_ptr);

        if (code) {
            if (toktype == KG_TOK_SEAL_MSG)
                gssalloc_free(token.value);
            *minor_status = code;
            return(GSS_S_FAILURE);
        }

        code = kg_encrypt_inplace(context, ctx->seq, KG_USAGE_SEAL,
                                  (g_OID_equal(ctx->mech_used,
                                               gss_mech_krb5_old) ?
                                   ctx->seq->keyblock.contents : NULL),
                                  md5cksum.contents, 16);
        if (code) {
            krb5_free_checksum_contents(context, &md5cksum);
            if (toktype == KG_TOK_SEAL_MSG)
                gssalloc_free(token.value);
            *minor_status = code;
            return GSS_S_FAILURE;
        }

        if (signalg == 0)
            cksum.length = 8;
        else
            cksum.length = 16;
        cksum.contents = md5cksum.contents + 16 - cksum.length;

        code = k5_bcmp(cksum.contents, ptr + 14, cksum.length);
        break;

    case SGN_ALG_MD2_5:
        if (!ctx->seed_init &&
            (code = kg_make_seed(context, ctx->subkey, ctx->seed))) {
            krb5_free_checksum_contents(context, &md5cksum);
            if (sealalg != 0xffff)
                xfree(plain);
            if (toktype == KG_TOK_SEAL_MSG)
                gssalloc_free(token.value);
            *minor_status = code;
            return GSS_S_FAILURE;
        }

        if (! (data_ptr = xmalloc(sizeof(ctx->seed) + 8 + plainlen))) {
            krb5_free_checksum_contents(context, &md5cksum);
            if (sealalg == 0)
                xfree(plain);
            if (toktype == KG_TOK_SEAL_MSG)
                gssalloc_free(token.value);
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }
        (void) memcpy(data_ptr, ptr-2, 8);
        (void) memcpy(data_ptr+8, ctx->seed, sizeof(ctx->seed));
        (void) memcpy(data_ptr+8+sizeof(ctx->seed), plain, plainlen);
        plaind.length = 8 + sizeof(ctx->seed) + plainlen;
        plaind.data = data_ptr;
        krb5_free_checksum_contents(context, &md5cksum);
        code = krb5_k_make_checksum(context, md5cksum.checksum_type,
                                    ctx->seq, sign_usage,
                                    &plaind, &md5cksum);
        xfree(data_ptr);

        if (code) {
            if (sealalg == 0)
                xfree(plain);
            if (toktype == KG_TOK_SEAL_MSG)
                gssalloc_free(token.value);
            *minor_status = code;
            return(GSS_S_FAILURE);
        }

        code = k5_bcmp(md5cksum.contents, ptr + 14, 8);
        /* Falls through to defective-token??  */

    default:
        *minor_status = 0;
        return(GSS_S_DEFECTIVE_TOKEN);

    case SGN_ALG_HMAC_SHA1_DES3_KD:
    case SGN_ALG_HMAC_MD5:
        /* compute the checksum of the message */

        /* 8 = bytes of token body to be checksummed according to spec */

        if (! (data_ptr = xmalloc(8 + plainlen))) {
            if (sealalg != 0xffff)
                xfree(plain);
            if (toktype == KG_TOK_SEAL_MSG)
                gssalloc_free(token.value);
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }

        (void) memcpy(data_ptr, ptr-2, 8);

        (void) memcpy(data_ptr+8, plain, plainlen);

        plaind.length = 8 + plainlen;
        plaind.data = data_ptr;
        code = krb5_k_make_checksum(context, md5cksum.checksum_type,
                                    ctx->seq, sign_usage,
                                    &plaind, &md5cksum);
        xfree(data_ptr);

        if (code) {
            if (toktype == KG_TOK_SEAL_MSG)
                gssalloc_free(token.value);
            *minor_status = code;
            return(GSS_S_FAILURE);
        }

        code = k5_bcmp(md5cksum.contents, ptr + 14, cksum_len);
        break;
    }

    krb5_free_checksum_contents(context, &md5cksum);
    if (sealalg != 0xffff)
        xfree(plain);

    /* compare the computed checksum against the transmitted checksum */

    if (code || bad_pad) {
        if (toktype == KG_TOK_SEAL_MSG)
            gssalloc_free(token.value);
        *minor_status = 0;
        return(GSS_S_BAD_SIG);
    }


    /* It got through unscathed.  Make sure the context is unexpired. */

    if (toktype == KG_TOK_SEAL_MSG)
        *message_buffer = token;

    if (conf_state)
        *conf_state = (sealalg != 0xffff);

    if (qop_state)
        *qop_state = GSS_C_QOP_DEFAULT;

    /* do sequencing checks */

    if ((ctx->initiate && direction != 0xff) ||
        (!ctx->initiate && direction != 0)) {
        if (toktype == KG_TOK_SEAL_MSG) {
            gssalloc_free(token.value);
            message_buffer->value = NULL;
            message_buffer->length = 0;
        }
        *minor_status = (OM_uint32)G_BAD_DIRECTION;
        return(GSS_S_BAD_SIG);
    }

    retval = g_seqstate_check(ctx->seqstate, (uint64_t)seqnum);

    /* success or ordering violation */

    *minor_status = 0;
    return(retval);
}

/* message_buffer is an input if SIGN, output if SEAL, and ignored if DEL_CTX
   conf_state is only valid if SEAL. */

OM_uint32
kg_unseal(minor_status, context_handle, input_token_buffer,
          message_buffer, conf_state, qop_state, toktype)
    OM_uint32 *minor_status;
    gss_ctx_id_t context_handle;
    gss_buffer_t input_token_buffer;
    gss_buffer_t message_buffer;
    int *conf_state;
    gss_qop_t *qop_state;
    int toktype;
{
    krb5_gss_ctx_id_rec *ctx;
    unsigned char *ptr;
    unsigned int bodysize;
    int err;
    int toktype2;
    int vfyflags = 0;
    OM_uint32 ret;

    ctx = (krb5_gss_ctx_id_rec *) context_handle;

    if (ctx->terminated || !ctx->established) {
        *minor_status = KG_CTX_INCOMPLETE;
        return(GSS_S_NO_CONTEXT);
    }

    /* parse the token, leave the data in message_buffer, setting conf_state */

    /* verify the header */

    ptr = (unsigned char *) input_token_buffer->value;


    err = g_verify_token_header(ctx->mech_used,
                                &bodysize, &ptr, -1,
                                input_token_buffer->length,
                                vfyflags);
    if (err) {
        *minor_status = err;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if (bodysize < 2) {
        *minor_status = (OM_uint32)G_BAD_TOK_HEADER;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    toktype2 = load_16_be(ptr);

    ptr += 2;
    bodysize -= 2;

    switch (toktype2) {
    case KG2_TOK_MIC_MSG:
    case KG2_TOK_WRAP_MSG:
    case KG2_TOK_DEL_CTX:
        ret = gss_krb5int_unseal_token_v3(&ctx->k5_context, minor_status, ctx,
                                          ptr, bodysize, message_buffer,
                                          conf_state, qop_state, toktype);
        break;
    case KG_TOK_MIC_MSG:
    case KG_TOK_WRAP_MSG:
    case KG_TOK_DEL_CTX:
        ret = kg_unseal_v1(ctx->k5_context, minor_status, ctx, ptr, bodysize,
                           message_buffer, conf_state, qop_state,
                           toktype);
        break;
    default:
        *minor_status = (OM_uint32)G_BAD_TOK_HEADER;
        ret = GSS_S_DEFECTIVE_TOKEN;
        break;
    }

    if (ret != 0)
        save_error_info (*minor_status, ctx->k5_context);

    return ret;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_unwrap(minor_status, context_handle,
                input_message_buffer, output_message_buffer,
                conf_state, qop_state)
    OM_uint32           *minor_status;
    gss_ctx_id_t        context_handle;
    gss_buffer_t        input_message_buffer;
    gss_buffer_t        output_message_buffer;
    int                 *conf_state;
    gss_qop_t           *qop_state;
{
    OM_uint32           rstat;

    rstat = kg_unseal(minor_status, context_handle,
                      input_message_buffer, output_message_buffer,
                      conf_state, qop_state, KG_TOK_WRAP_MSG);
    return(rstat);
}

OM_uint32 KRB5_CALLCONV
krb5_gss_verify_mic(minor_status, context_handle,
                    message_buffer, token_buffer,
                    qop_state)
    OM_uint32           *minor_status;
    gss_ctx_id_t        context_handle;
    gss_buffer_t        message_buffer;
    gss_buffer_t        token_buffer;
    gss_qop_t           *qop_state;
{
    OM_uint32           rstat;

    rstat = kg_unseal(minor_status, context_handle,
                      token_buffer, message_buffer,
                      NULL, qop_state, KG_TOK_MIC_MSG);
    return(rstat);
}
