/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_invalid.c - Invalid message token regression tests */
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
 * This file contains regression tests for some GSSAPI invalid token
 * vulnerabilities.
 *
 * 1. A pre-CFX wrap or MIC token processed with a CFX-only context causes a
 *    null pointer dereference.  (The token must use SEAL_ALG_NONE or it will
 *    be rejected.)
 *
 * 2. A pre-CFX wrap or MIC token with fewer than 24 bytes after the ASN.1
 *    header causes an input buffer overrun, usually leading to either a segv
 *    or a GSS_S_DEFECTIVE_TOKEN error due to garbage algorithm, filler, or
 *    sequence number values.
 *
 * 3. A pre-CFX wrap token with fewer than 16 + cksumlen bytes after the ASN.1
 *    header causes an integer underflow when computing the ciphertext length,
 *    leading to an allocation error on 32-bit platforms or a segv on 64-bit
 *    platforms.  A pre-CFX MIC token of this size causes an input buffer
 *    overrun when comparing the checksum, perhaps leading to a segv.
 *
 * 4. A pre-CFX wrap token with fewer than conflen + padlen bytes in the
 *    ciphertext (where padlen is the last byte of the decrypted ciphertext)
 *    causes an integer underflow when computing the original message length,
 *    leading to an allocation error.
 *
 * 5. In the mechglue, truncated encapsulation in the initial context token can
 *    cause input buffer overruns in gss_accept_sec_context().
 *
 * Vulnerabilities #1 and #2 also apply to IOV unwrap, although tokens with
 * fewer than 16 bytes after the ASN.1 header will be rejected.
 * Vulnerabilities #2 and #5 can only be robustly detected using a
 * memory-checking environment such as valgrind.
 */

#include "k5-int.h"
#include "common.h"
#include "mglueP.h"
#include "gssapiP_krb5.h"

/*
 * The following samples contain context parameters and otherwise valid seal
 * tokens where the plain text is padded with byte value 100 instead of the
 * proper value 1.
 */
struct test {
    krb5_enctype enctype;
    krb5_enctype encseq_enctype;
    int sealalg;
    int signalg;
    size_t cksum_size;
    size_t keylen;
    const char *keydata;
    size_t toklen;
    const char *token;
} tests[] = {
    {
        ENCTYPE_DES_CBC_CRC, ENCTYPE_DES_CBC_RAW,
        SEAL_ALG_DES, SGN_ALG_DES_MAC_MD5, 8,
        8,
        "\x26\xEC\xBA\xB6\xFE\xBA\x91\xCE",
        53,
        "\x60\x33\x06\x09\x2A\x86\x48\x86\xF7\x12\x01\x02\x02\x02\x01\x00"
        "\x00\x00\x00\xFF\xFF\xF0\x0B\x90\x7B\xC4\xFC\xEB\xF4\x84\x9C\x5A"
        "\xA8\x56\x41\x3E\xE1\x62\xEE\x38\xD1\x34\x9A\xE3\xFB\xC9\xFD\x0A"
        "\xDC\x83\xE1\x4A\xE4"
    },
    {
        ENCTYPE_DES3_CBC_SHA1, ENCTYPE_DES3_CBC_RAW,
        SEAL_ALG_DES3KD, SGN_ALG_HMAC_SHA1_DES3_KD, 20,
        24,
        "\x4F\xEA\x19\x19\x5E\x0E\x10\xDF\x3D\x29\xB5\x13\x8F\x01\xC7\xA7"
        "\x92\x3D\x38\xF7\x26\x73\x0D\x6D",
        65,
        "\x60\x3F\x06\x09\x2A\x86\x48\x86\xF7\x12\x01\x02\x02\x02\x01\x04"
        "\x00\x02\x00\xFF\xFF\xEB\xF3\x9A\x89\x24\x57\xB8\x63\x95\x25\xE8"
        "\x6E\x8E\x79\xE6\x2E\xCA\xD3\xFF\x57\x9F\x8C\xAB\xEF\xDD\x28\x10"
        "\x2F\x93\x21\x2E\xF2\x52\xB6\x6F\xA8\xBB\x8A\x6D\xAA\x6F\xB7\xF4\xD4"
    },
    {
        ENCTYPE_ARCFOUR_HMAC, ENCTYPE_ARCFOUR_HMAC,
        SEAL_ALG_MICROSOFT_RC4, SGN_ALG_HMAC_MD5, 8,
        16,
        "\x66\x64\x41\x64\x55\x78\x21\xD0\xD0\xFD\x05\x6A\xFF\x6F\xE8\x09",
        53,
        "\x60\x33\x06\x09\x2A\x86\x48\x86\xF7\x12\x01\x02\x02\x02\x01\x11"
        "\x00\x10\x00\xFF\xFF\x35\xD4\x79\xF3\x8C\x47\x8F\x6E\x23\x6F\x3E"
        "\xCC\x5E\x57\x5C\x6A\x89\xF0\xA2\x03\x4F\x0B\x51\x11\xEE\x89\x7E"
        "\xD6\xF6\xB5\xD6\x51"
    }
};

/* Fake up enough of a CFX GSS context for gss_unwrap, using an AES key. */
static gss_ctx_id_t
make_fake_cfx_context()
{
    gss_union_ctx_id_t uctx;
    krb5_gss_ctx_id_t kgctx;
    krb5_keyblock kb;

    kgctx = calloc(1, sizeof(*kgctx));
    if (kgctx == NULL)
        abort();
    kgctx->established = 1;
    kgctx->proto = 1;
    if (g_seqstate_init(&kgctx->seqstate, 0, 0, 0, 0) != 0)
        abort();
    kgctx->mech_used = &mech_krb5;
    kgctx->sealalg = -1;
    kgctx->signalg = -1;

    kb.enctype = ENCTYPE_AES128_CTS_HMAC_SHA1_96;
    kb.length = 16;
    kb.contents = (unsigned char *)"1234567887654321";
    if (krb5_k_create_key(NULL, &kb, &kgctx->subkey) != 0)
        abort();

    uctx = calloc(1, sizeof(*uctx));
    if (uctx == NULL)
        abort();
    uctx->mech_type = &mech_krb5;
    uctx->internal_ctx_id = (gss_ctx_id_t)kgctx;
    return (gss_ctx_id_t)uctx;
}

/* Fake up enough of a GSS context for gss_unwrap, using keys from test. */
static gss_ctx_id_t
make_fake_context(const struct test *test)
{
    gss_union_ctx_id_t uctx;
    krb5_gss_ctx_id_t kgctx;
    krb5_keyblock kb;
    unsigned char encbuf[8];
    size_t i;

    kgctx = calloc(1, sizeof(*kgctx));
    if (kgctx == NULL)
        abort();
    kgctx->established = 1;
    if (g_seqstate_init(&kgctx->seqstate, 0, 0, 0, 0) != 0)
        abort();
    kgctx->mech_used = &mech_krb5;
    kgctx->sealalg = test->sealalg;
    kgctx->signalg = test->signalg;
    kgctx->cksum_size = test->cksum_size;

    kb.enctype = test->enctype;
    kb.length = test->keylen;
    kb.contents = (unsigned char *)test->keydata;
    if (krb5_k_create_key(NULL, &kb, &kgctx->subkey) != 0)
        abort();

    kb.enctype = test->encseq_enctype;
    if (krb5_k_create_key(NULL, &kb, &kgctx->seq) != 0)
        abort();

    if (kb.enctype == ENCTYPE_DES_CBC_RAW) {
        for (i = 0; i < 8; i++)
            encbuf[i] = kb.contents[i] ^ 0xF0;
        kb.contents = encbuf;
    }
    if (krb5_k_create_key(NULL, &kb, &kgctx->enc) != 0)
        abort();

    uctx = calloc(1, sizeof(*uctx));
    if (uctx == NULL)
        abort();
    uctx->mech_type = &mech_krb5;
    uctx->internal_ctx_id = (gss_ctx_id_t)kgctx;
    return (gss_ctx_id_t)uctx;
}

/* Free a context created by make_fake_context. */
static void
free_fake_context(gss_ctx_id_t ctx)
{
    gss_union_ctx_id_t uctx = (gss_union_ctx_id_t)ctx;
    krb5_gss_ctx_id_t kgctx = (krb5_gss_ctx_id_t)uctx->internal_ctx_id;

    free(kgctx->seqstate);
    krb5_k_free_key(NULL, kgctx->subkey);
    krb5_k_free_key(NULL, kgctx->seq);
    krb5_k_free_key(NULL, kgctx->enc);
    free(kgctx);
    free(uctx);
}

/* Prefix a token (starting at the two-byte ID) with an ASN.1 header and return
 * it in an allocated block to facilitate checking by valgrind or similar. */
static void
make_token(unsigned char *token, size_t len, gss_buffer_t out)
{
    char *wrapped;

    assert(mech_krb5.length == 9);
    assert(len + 11 < 128);
    wrapped = malloc(len + 13);
    if (wrapped == NULL)
        abort();
    wrapped[0] = 0x60;
    wrapped[1] = len + 11;
    wrapped[2] = 0x06;
    wrapped[3] = 9;
    memcpy(wrapped + 4, mech_krb5.elements, 9);
    memcpy(wrapped + 13, token, len);
    out->length = len + 13;
    out->value = wrapped;
}

/* Unwrap a superficially valid RFC 1964 token with a CFX-only context, with
 * regular and IOV unwrap. */
static void
test_bogus_1964_token(gss_ctx_id_t ctx)
{
    OM_uint32 minor, major;
    unsigned char tokbuf[128];
    gss_buffer_desc in, out;
    gss_iov_buffer_desc iov;

    store_16_be(KG_TOK_SIGN_MSG, tokbuf);
    store_16_le(SGN_ALG_DES_MAC_MD5, tokbuf + 2);
    store_16_le(SEAL_ALG_NONE, tokbuf + 4);
    store_16_le(0xFFFF, tokbuf + 6);
    memset(tokbuf + 8, 0, 16);
    make_token(tokbuf, 24, &in);

    major = gss_unwrap(&minor, ctx, &in, &out, NULL, NULL);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    (void)gss_release_buffer(&minor, &out);

    iov.type = GSS_IOV_BUFFER_TYPE_HEADER;
    iov.buffer = in;
    major = gss_unwrap_iov(&minor, ctx, NULL, NULL, &iov, 1);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();

    free(in.value);
}

/* Process wrap and MIC tokens with incomplete headers. */
static void
test_short_header(gss_ctx_id_t ctx)
{
    OM_uint32 minor, major;
    unsigned char tokbuf[128];
    gss_buffer_desc in, out, empty = GSS_C_EMPTY_BUFFER;

    /* Seal token, 2-24 bytes */
    store_16_be(KG_TOK_SEAL_MSG, tokbuf);
    make_token(tokbuf, 2, &in);
    major = gss_unwrap(&minor, ctx, &in, &out, NULL, NULL);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    free(in.value);
    (void)gss_release_buffer(&minor, &out);

    /* Sign token, 2-24 bytes */
    store_16_be(KG_TOK_SIGN_MSG, tokbuf);
    make_token(tokbuf, 2, &in);
    major = gss_unwrap(&minor, ctx, &in, &out, NULL, NULL);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    free(in.value);
    (void)gss_release_buffer(&minor, &out);

    /* MIC token, 2-24 bytes */
    store_16_be(KG_TOK_MIC_MSG, tokbuf);
    make_token(tokbuf, 2, &in);
    major = gss_verify_mic(&minor, ctx, &empty, &in, NULL);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    free(in.value);
}

/* Process wrap and MIC tokens with incomplete headers. */
static void
test_short_header_iov(gss_ctx_id_t ctx, const struct test *test)
{
    OM_uint32 minor, major;
    unsigned char tokbuf[128];
    gss_iov_buffer_desc iov;

    /* IOV seal token, 16-23 bytes */
    store_16_be(KG_TOK_SEAL_MSG, tokbuf);
    store_16_le(test->signalg, tokbuf + 2);
    store_16_le(test->sealalg, tokbuf + 4);
    store_16_be(0xFFFF, tokbuf + 6);
    memset(tokbuf + 8, 0, 8);
    iov.type = GSS_IOV_BUFFER_TYPE_HEADER;
    make_token(tokbuf, 16, &iov.buffer);
    major = gss_unwrap_iov(&minor, ctx, NULL, NULL, &iov, 1);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    free(iov.buffer.value);

    /* IOV sign token, 16-23 bytes */
    store_16_be(KG_TOK_SIGN_MSG, tokbuf);
    store_16_le(test->signalg, tokbuf + 2);
    store_16_le(SEAL_ALG_NONE, tokbuf + 4);
    store_16_le(0xFFFF, tokbuf + 6);
    memset(tokbuf + 8, 0, 8);
    iov.type = GSS_IOV_BUFFER_TYPE_HEADER;
    make_token(tokbuf, 16, &iov.buffer);
    major = gss_unwrap_iov(&minor, ctx, NULL, NULL, &iov, 1);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    free(iov.buffer.value);

    /* IOV MIC token, 16-23 bytes */
    store_16_be(KG_TOK_MIC_MSG, tokbuf);
    store_16_be(test->signalg, tokbuf + 2);
    store_16_le(SEAL_ALG_NONE, tokbuf + 4);
    store_16_le(0xFFFF, tokbuf + 6);
    memset(tokbuf + 8, 0, 8);
    iov.type = GSS_IOV_BUFFER_TYPE_MIC_TOKEN;
    make_token(tokbuf, 16, &iov.buffer);
    major = gss_verify_mic_iov(&minor, ctx, NULL, &iov, 1);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    free(iov.buffer.value);
}

/* Process wrap and MIC tokens with incomplete checksums. */
static void
test_short_checksum(gss_ctx_id_t ctx, const struct test *test)
{
    OM_uint32 minor, major;
    unsigned char tokbuf[128];
    gss_buffer_desc in, out, empty = GSS_C_EMPTY_BUFFER;

    /* Can only do this with the DES3 checksum, as we can't easily get past
     * retrieving the sequence number when the checksum is only eight bytes. */
    if (test->cksum_size <= 8)
        return;
    /* Seal token, fewer than 16 + cksum_size bytes.  Use the token from the
     * test data to get a valid sequence number. */
    make_token((unsigned char *)test->token + 13, 24, &in);
    major = gss_unwrap(&minor, ctx, &in, &out, NULL, NULL);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    free(in.value);
    (void)gss_release_buffer(&minor, &out);

    /* Sign token, fewer than 16 + cksum_size bytes. */
    memcpy(tokbuf, test->token + 13, 24);
    store_16_be(KG_TOK_SIGN_MSG, tokbuf);
    store_16_le(SEAL_ALG_NONE, tokbuf + 4);
    make_token(tokbuf, 24, &in);
    major = gss_unwrap(&minor, ctx, &in, &out, NULL, NULL);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    free(in.value);
    (void)gss_release_buffer(&minor, &out);

    /* MIC token, fewer than 16 + cksum_size bytes. */
    memcpy(tokbuf, test->token + 13, 24);
    store_16_be(KG_TOK_MIC_MSG, tokbuf);
    store_16_le(SEAL_ALG_NONE, tokbuf + 4);
    make_token(tokbuf, 24, &in);
    major = gss_verify_mic(&minor, ctx, &empty, &in, NULL);
    if (major != GSS_S_DEFECTIVE_TOKEN)
        abort();
    free(in.value);
}

/* Unwrap a token with a bogus padding byte in the decrypted ciphertext. */
static void
test_bad_pad(gss_ctx_id_t ctx, const struct test *test)
{
    OM_uint32 minor, major;
    gss_buffer_desc in, out;

    in.length = test->toklen;
    in.value = (char *)test->token;
    major = gss_unwrap(&minor, ctx, &in, &out, NULL, NULL);
    if (major != GSS_S_BAD_SIG)
        abort();
    (void)gss_release_buffer(&minor, &out);
}

static void
try_accept(void *value, size_t len)
{
    OM_uint32 minor;
    gss_buffer_desc in, out;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;

    /* Copy the provided value to make input overruns more obvious. */
    in.value = malloc(len);
    if (in.value == NULL)
        abort();
    memcpy(in.value, value, len);
    in.length = len;
    (void)gss_accept_sec_context(&minor, &ctx, GSS_C_NO_CREDENTIAL, &in,
                                 GSS_C_NO_CHANNEL_BINDINGS, NULL, NULL,
                                 &out, NULL, NULL, NULL);
    gss_release_buffer(&minor, &out);
    gss_delete_sec_context(&minor, &ctx, GSS_C_NO_BUFFER);
    free(in.value);
}

/* Accept contexts using superficially valid but truncated encapsulations. */
static void
test_short_encapsulation()
{
    /* Include just the initial application tag, to see if we overrun reading
     * the sequence length. */
    try_accept("\x60", 1);

    /* Indicate four additional sequence length bytes, to see if we overrun
     * reading them (or skipping them and reading the next byte). */
    try_accept("\x60\x84", 2);

    /* Include an object identifier tag but no length, to see if we overrun
     * reading the length. */
    try_accept("\x60\x40\x06", 3);

    /* Include an object identifier tag with a length matching the krb5 mech,
     * but no OID bytes, to see if we overrun comparing against mechs. */
    try_accept("\x60\x40\x06\x09", 4);
}

int
main(int argc, char **argv)
{
    gss_ctx_id_t ctx;
    size_t i;

    ctx = make_fake_cfx_context();
    test_bogus_1964_token(ctx);
    free_fake_context(ctx);

    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        ctx = make_fake_context(&tests[i]);
        test_short_header(ctx);
        test_short_header_iov(ctx, &tests[i]);
        test_short_checksum(ctx, &tests[i]);
        test_bad_pad(ctx, &tests[i]);
        free_fake_context(ctx);
    }

    test_short_encapsulation();

    return 0;
}
