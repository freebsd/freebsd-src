/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_pcontok.c - gss_process_context_token tests */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
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

/*
 * This test program exercises krb5 gss_process_context_token.  It first
 * establishes a context to a named target.  Then, if the resulting context
 * uses RFC 1964, it creates a context deletion token from the acceptor to the
 * initiator and passes it to the initiator using gss_process_context_token.
 * If the established context uses RFC 4121, this program feeds a made-up
 * context token to gss_process_context_token and checks for the expected
 * error.
 */

#include "k5-int.h"
#include "common.h"

#define SGN_ALG_DES_MAC_MD5       0x00
#define SGN_ALG_HMAC_SHA1_DES3_KD 0x04
#define SGN_ALG_HMAC_MD5          0x11

/*
 * Create a valid RFC 1964 context deletion token using the information in *
 * lctx.  We must do this by hand since we no longer create context deletion
 * tokens from gss_delete_sec_context.
 */
static void
make_delete_token(gss_krb5_lucid_context_v1_t *lctx, gss_buffer_desc *out)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_keyblock seqkb;
    krb5_key seq;
    krb5_checksum cksum;
    krb5_cksumtype cktype;
    krb5_keyusage ckusage;
    krb5_crypto_iov iov;
    krb5_data d;
    size_t cksize, tlen;
    unsigned char *token, *ptr, iv[8];
    gss_krb5_lucid_key_t *lkey = &lctx->rfc1964_kd.ctx_key;
    int signalg = lctx->rfc1964_kd.sign_alg;

    ret = krb5_init_context(&context);
    check_k5err(context, "krb5_init_context", ret);

    seqkb.enctype = lkey->type;
    seqkb.length = lkey->length;
    seqkb.contents = lkey->data;
    ret = krb5_k_create_key(context, &seqkb, &seq);
    check_k5err(context, "krb5_k_create_key", ret);

    if (signalg == SGN_ALG_DES_MAC_MD5) {
        cktype = CKSUMTYPE_RSA_MD5;
        cksize = 8;
        ckusage = 0;
    } else if (signalg == SGN_ALG_HMAC_SHA1_DES3_KD) {
        cktype = CKSUMTYPE_HMAC_SHA1_DES3;
        cksize = 20;
        ckusage = 23;
    } else if (signalg == SGN_ALG_HMAC_MD5) {
        cktype = CKSUMTYPE_HMAC_MD5_ARCFOUR;
        cksize = 8;
        ckusage = 15;
    } else {
        abort();
    }

    tlen = 20 + mech_krb5.length + cksize;
    token = malloc(tlen);
    assert(token != NULL);

    /* Create the ASN.1 wrapper (4 + mech_krb5.length bytes).  Assume the ASN.1
     * lengths fit in one byte since deletion tokens are short. */
    ptr = token;
    *ptr++ = 0x60;
    *ptr++ = tlen - 2;
    *ptr++ = 0x06;
    *ptr++ = mech_krb5.length;
    memcpy(ptr, mech_krb5.elements, mech_krb5.length);
    ptr += mech_krb5.length;

    /* Create the RFC 1964 token header (8 bytes). */
    *ptr++ = 0x01;
    *ptr++ = 0x02;
    store_16_le(signalg, ptr);
    ptr += 2;
    *ptr++ = 0xFF;
    *ptr++ = 0xFF;
    *ptr++ = 0xFF;
    *ptr++ = 0xFF;

    /* Create the checksum (cksize bytes at offset 8 from the header). */
    d = make_data(ptr - 8, 8);
    ret = krb5_k_make_checksum(context, cktype, seq, ckusage, &d, &cksum);
    check_k5err(context, "krb5_k_make_checksum", ret);
    if (signalg == SGN_ALG_DES_MAC_MD5) {
        iov.flags = KRB5_CRYPTO_TYPE_DATA;
        iov.data = make_data(cksum.contents, 16);
        ret = krb5_k_encrypt_iov(context, seq, 0, NULL, &iov, 1);
        memcpy(ptr + 8, cksum.contents + 8, 8);
    } else {
        memcpy(ptr + 8, cksum.contents, cksize);
    }

    /* Create the sequence number (8 bytes). */
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = make_data(ptr, 8);
    ptr[4] = ptr[5] = ptr[6] = ptr[7] = lctx->initiate ? 0 : 0xFF;
    memcpy(iv, ptr + 8, 8);
    d = make_data(iv, 8);
    if (signalg == SGN_ALG_HMAC_MD5) {
        store_32_be(lctx->send_seq, ptr);
        ret = krb5int_arcfour_gsscrypt(&seq->keyblock, 0, &d, &iov, 1);
        check_k5err(context, "krb5int_arcfour_gsscrypt(seq)", ret);
    } else {
        store_32_le(lctx->send_seq, ptr);
        ret = krb5_k_encrypt_iov(context, seq, 24, &d, &iov, 1);
        check_k5err(context, "krb5_k_encrypt_iov(seq)", ret);
    }

    krb5_free_checksum_contents(context, &cksum);
    krb5_k_free_key(context, seq);
    krb5_free_context(context);

    out->length = tlen;
    out->value = token;
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major, flags;
    gss_name_t tname;
    gss_buffer_desc token, in = GSS_C_EMPTY_BUFFER, out;
    gss_ctx_id_t ictx, actx;
    gss_krb5_lucid_context_v1_t *lctx;
    void *lptr;

    assert(argc == 2);
    tname = import_name(argv[1]);

    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG;
    establish_contexts(&mech_krb5, GSS_C_NO_CREDENTIAL, GSS_C_NO_CREDENTIAL,
                       tname, flags, &ictx, &actx, NULL, NULL, NULL);

    /* Export the acceptor context to a lucid context so we can look inside. */
    major = gss_krb5_export_lucid_sec_context(&minor, &actx, 1, &lptr);
    check_gsserr("gss_export_lucid_sec_context", major, minor);
    lctx = lptr;
    if (!lctx->protocol) {
        /* Make an RFC 1964 context deletion token and pass it to
         * gss_process_context_token. */
        make_delete_token(lctx, &token);
        major = gss_process_context_token(&minor, ictx, &token);
        free(token.value);
        check_gsserr("gss_process_context_token", major, minor);
        /* Check for the appropriate major code from gss_wrap. */
        major = gss_wrap(&minor, ictx, 1, GSS_C_QOP_DEFAULT, &in, NULL, &out);
        assert(major == GSS_S_NO_CONTEXT);
    } else {
        /* RFC 4121 defines no context deletion token, so try passing something
         * arbitrary and check for the appropriate major code. */
        token.value = "abcd";
        token.length = 4;
        major = gss_process_context_token(&minor, ictx, &token);
        assert(major == GSS_S_DEFECTIVE_TOKEN);
    }

    (void)gss_release_name(&minor, &tname);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_krb5_free_lucid_sec_context(&minor, lptr);
    return 0;
}
