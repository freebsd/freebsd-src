/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_iov.c - Test program for IOV functions */
/*
 * Copyright (C) 2013 by the Massachusetts Institute of Technology.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "common.h"

/* Concatenate iov (except for sign-only buffers) into a contiguous token. */
static void
concat_iov(gss_iov_buffer_desc *iov, size_t iovlen, char **buf_out,
           size_t *len_out)
{
    size_t len, i;
    char *buf;

    /* Concatenate the result into a contiguous buffer. */
    len = 0;
    for (i = 0; i < iovlen; i++) {
        if (GSS_IOV_BUFFER_TYPE(iov[i].type) != GSS_IOV_BUFFER_TYPE_SIGN_ONLY)
            len += iov[i].buffer.length;
    }
    buf = malloc(len);
    if (buf == NULL)
        errout("malloc failed");
    len = 0;
    for (i = 0; i < iovlen; i++) {
        if (GSS_IOV_BUFFER_TYPE(iov[i].type) == GSS_IOV_BUFFER_TYPE_SIGN_ONLY)
            continue;
        memcpy(buf + len, iov[i].buffer.value, iov[i].buffer.length);
        len += iov[i].buffer.length;
    }
    *buf_out = buf;
    *len_out = len;
}

static void
check_encrypted(const char *msg, int conf, const char *buf, const char *plain)
{
    int same = memcmp(buf, plain, strlen(plain)) == 0;

    if ((conf && same) || (!conf && !same))
        errout(msg);
}

/*
 * Wrap str in standard form (HEADER | DATA | PADDING | TRAILER) using the
 * caller-provided array iov, which must have space for four elements.  Library
 * allocation will be used for the header/padding/trailer buffers, so the
 * caller must check and free them.
 */
static void
wrap_std(gss_ctx_id_t ctx, char *str, gss_iov_buffer_desc *iov, int conf)
{
    OM_uint32 minor, major;
    int oconf;

    /* Lay out iov array. */
    iov[0].type = GSS_IOV_BUFFER_TYPE_HEADER | GSS_IOV_BUFFER_FLAG_ALLOCATE;
    iov[1].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[1].buffer.value = str;
    iov[1].buffer.length = strlen(str);
    iov[2].type = GSS_IOV_BUFFER_TYPE_PADDING | GSS_IOV_BUFFER_FLAG_ALLOCATE;
    iov[3].type = GSS_IOV_BUFFER_TYPE_TRAILER | GSS_IOV_BUFFER_FLAG_ALLOCATE;

    /* Wrap.  This will allocate header/padding/trailer buffers as necessary
     * and encrypt str in place. */
    major = gss_wrap_iov(&minor, ctx, conf, GSS_C_QOP_DEFAULT, &oconf, iov, 4);
    check_gsserr("gss_wrap_iov(std)", major, minor);
    if (oconf != conf)
        errout("gss_wrap_iov(std) conf");
}

/* Create standard tokens using gss_wrap_iov and ctx1, and make sure we can
 * unwrap them using ctx2 in all of the supported ways. */
static void
test_standard_wrap(gss_ctx_id_t ctx1, gss_ctx_id_t ctx2, int conf)
{
    OM_uint32 major, minor;
    gss_iov_buffer_desc iov[4], stiov[2];
    gss_qop_t qop;
    gss_buffer_desc input, output;
    const char *string1 = "The swift brown fox jumped over the lazy dog.";
    const char *string2 = "Now is the time!";
    const char *string3 = "x";
    const char *string4 = "!@#";
    char data[1024], *fulltoken;
    size_t len;
    int oconf;
    ptrdiff_t offset;

    /* Wrap a standard token and unwrap it using the iov array. */
    memcpy(data, string1, strlen(string1) + 1);
    wrap_std(ctx1, data, iov, conf);
    check_encrypted("gss_wrap_iov(std1) encryption", conf, data, string1);
    major = gss_unwrap_iov(&minor, ctx2, &oconf, &qop, iov, 4);
    check_gsserr("gss_unwrap_iov(std1)", major, minor);
    if (oconf != conf || qop != GSS_C_QOP_DEFAULT)
        errout("gss_unwrap_iov(std1) conf/qop");
    if (iov[1].buffer.value != data || iov[1].buffer.length != strlen(string1))
        errout("gss_unwrap_iov(std1) data buffer");
    if (memcmp(data, string1, iov[1].buffer.length) != 0)
        errout("gss_unwrap_iov(std1) decryption");
    (void)gss_release_iov_buffer(&minor, iov, 4);

    /* Wrap a standard token and unwrap it using gss_unwrap(). */
    memcpy(data, string2, strlen(string2) + 1);
    wrap_std(ctx1, data, iov, conf);
    concat_iov(iov, 4, &fulltoken, &len);
    input.value = fulltoken;
    input.length = len;
    major = gss_unwrap(&minor, ctx2, &input, &output, &oconf, &qop);
    check_gsserr("gss_unwrap(std2)", major, minor);
    if (oconf != conf || qop != GSS_C_QOP_DEFAULT)
        errout("gss_unwrap(std2) conf/qop");
    if (output.length != strlen(string2) ||
        memcmp(output.value, string2, output.length) != 0)
        errout("gss_unwrap(std2) decryption");
    (void)gss_release_buffer(&minor, &output);
    (void)gss_release_iov_buffer(&minor, iov, 4);
    free(fulltoken);

    /* Wrap a standard token and unwrap it using a stream buffer. */
    memcpy(data, string3, strlen(string3) + 1);
    wrap_std(ctx1, data, iov, conf);
    concat_iov(iov, 4, &fulltoken, &len);
    stiov[0].type = GSS_IOV_BUFFER_TYPE_STREAM;
    stiov[0].buffer.value = fulltoken;
    stiov[0].buffer.length = len;
    stiov[1].type = GSS_IOV_BUFFER_TYPE_DATA;
    major = gss_unwrap_iov(&minor, ctx2, &oconf, &qop, stiov, 2);
    check_gsserr("gss_unwrap_iov(std3)", major, minor);
    if (oconf != conf || qop != GSS_C_QOP_DEFAULT)
        errout("gss_unwrap_iov(std3) conf/qop");
    if (stiov[1].buffer.length != strlen(string3) ||
        memcmp(stiov[1].buffer.value, string3, strlen(string3)) != 0)
        errout("gss_unwrap_iov(std3) decryption");
    offset = (char *)stiov[1].buffer.value - fulltoken;
    if (offset < 0 || (size_t)offset > len)
        errout("gss_unwrap_iov(std3) offset");
    (void)gss_release_iov_buffer(&minor, iov, 4);
    free(fulltoken);

    /* Wrap a token using gss_wrap and unwrap it using a stream buffer with
     * allocation and copying. */
    input.value = (char *)string4;
    input.length = strlen(string4);
    major = gss_wrap(&minor, ctx1, conf, GSS_C_QOP_DEFAULT, &input, &oconf,
                     &output);
    check_gsserr("gss_wrap(std4)", major, minor);
    if (oconf != conf)
        errout("gss_wrap(std4) conf");
    stiov[0].type = GSS_IOV_BUFFER_TYPE_STREAM;
    stiov[0].buffer = output;
    stiov[1].type = GSS_IOV_BUFFER_TYPE_DATA | GSS_IOV_BUFFER_FLAG_ALLOCATE;
    major = gss_unwrap_iov(&minor, ctx2, &oconf, &qop, stiov, 2);
    check_gsserr("gss_unwrap_iov(std4)", major, minor);
    if (!(GSS_IOV_BUFFER_FLAGS(stiov[1].type) & GSS_IOV_BUFFER_FLAG_ALLOCATED))
        errout("gss_unwrap_iov(std4) allocated");
    if (oconf != conf || qop != GSS_C_QOP_DEFAULT)
        errout("gss_unwrap_iov(std4) conf/qop");
    if (stiov[1].buffer.length != strlen(string4) ||
        memcmp(stiov[1].buffer.value, string4, strlen(string4)) != 0)
        errout("gss_unwrap_iov(std4) decryption");
    (void)gss_release_buffer(&minor, &output);
    (void)gss_release_iov_buffer(&minor, stiov, 2);
}

/*
 * Wrap an AEAD token (HEADER | SIGN_ONLY | DATA | PADDING | TRAILER) using the
 * caller-provided array iov, which must have space for five elements, and the
 * caller-provided buffer data, which must be big enough to handle the test
 * inputs.  Library allocation will not be used.
 */
static void
wrap_aead(gss_ctx_id_t ctx, const char *sign, const char *wrap,
          gss_iov_buffer_desc *iov, char *data, int conf)
{
    OM_uint32 major, minor;
    int oconf;
    char *ptr;

    /* Lay out iov array. */
    iov[0].type = GSS_IOV_BUFFER_TYPE_HEADER;
    iov[1].type = GSS_IOV_BUFFER_TYPE_SIGN_ONLY;
    iov[1].buffer.value = (char *)sign;
    iov[1].buffer.length = strlen(sign);
    iov[2].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[2].buffer.value = (char *)wrap;
    iov[2].buffer.length = strlen(wrap);
    iov[3].type = GSS_IOV_BUFFER_TYPE_PADDING;
    iov[4].type = GSS_IOV_BUFFER_TYPE_TRAILER;

    /* Get header/padding/trailer lengths. */
    major = gss_wrap_iov_length(&minor, ctx, conf, GSS_C_QOP_DEFAULT, &oconf,
                                iov, 5);
    check_gsserr("gss_wrap_iov_length(aead)", major, minor);
    if (oconf != conf)
        errout("gss_wrap_iov_length(aead) conf");
    if (iov[1].buffer.value != sign || iov[1].buffer.length != strlen(sign))
        errout("gss_wrap_iov_length(aead) sign-only buffer");
    if (iov[2].buffer.value != wrap || iov[2].buffer.length != strlen(wrap))
        errout("gss_wrap_iov_length(aead) data buffer");

    /* Set iov buffer pointers using returned lengths. */
    iov[0].buffer.value = data;
    ptr = data + iov[0].buffer.length;
    memcpy(ptr, wrap, strlen(wrap));
    iov[2].buffer.value = ptr;
    ptr += iov[2].buffer.length;
    iov[3].buffer.value = ptr;
    ptr += iov[3].buffer.length;
    iov[4].buffer.value = ptr;

    /* Wrap the AEAD token. */
    major = gss_wrap_iov(&minor, ctx, conf, GSS_C_QOP_DEFAULT, &oconf, iov, 5);
    check_gsserr("gss_wrap_iov(aead)", major, minor);
    if (oconf != conf)
        errout("gss_wrap_iov(aead) conf");
    if (iov[1].buffer.value != sign || iov[1].buffer.length != strlen(sign))
        errout("gss_wrap_iov(aead) sign-only buffer");
    if (iov[2].buffer.length != strlen(wrap))
        errout("gss_wrap_iov(aead) data buffer");
    check_encrypted("gss_wrap_iov(aead) encryption", conf, iov[2].buffer.value,
                    wrap);
}

/* Create AEAD tokens using gss_wrap_iov and ctx1, and make sure we can unwrap
 * them using ctx2 in all of the supported ways. */
static void
test_aead(gss_ctx_id_t ctx1, gss_ctx_id_t ctx2, int conf)
{
    OM_uint32 major, minor;
    gss_iov_buffer_desc iov[5], stiov[3];
    gss_qop_t qop;
    gss_buffer_desc input, assoc, output;
    const char *sign = "This data is only signed.";
    const char *wrap = "This data is wrapped in-place.";
    char data[1024], *fulltoken;
    size_t len;
    int oconf;
    ptrdiff_t offset;

    /* Wrap an AEAD token and unwrap it using the IOV array. */
    wrap_aead(ctx1, sign, wrap, iov, data, conf);
    major = gss_unwrap_iov(&minor, ctx2, &oconf, &qop, iov, 5);
    check_gsserr("gss_unwrap_iov(aead1)", major, minor);
    if (oconf != conf || qop != GSS_C_QOP_DEFAULT)
        errout("gss_unwrap_iov(aead1) conf/qop");
    if (iov[1].buffer.value != sign || iov[1].buffer.length != strlen(sign))
        errout("gss_unwrap_iov(aead1) sign-only buffer");
    if (iov[2].buffer.length != strlen(wrap) ||
        memcmp(iov[2].buffer.value, wrap, iov[2].buffer.length) != 0)
        errout("gss_unwrap_iov(aead1) decryption");

    /* Wrap an AEAD token and unwrap it using gss_unwrap_aead. */
    wrap_aead(ctx1, sign, wrap, iov, data, conf);
    concat_iov(iov, 5, &fulltoken, &len);
    input.value = fulltoken;
    input.length = len;
    assoc.value = (char *)sign;
    assoc.length = strlen(sign);
    major = gss_unwrap_aead(&minor, ctx2, &input, &assoc, &output, &oconf,
                            &qop);
    check_gsserr("gss_unwrap_aead(aead2)", major, minor);
    if (output.length != strlen(wrap) ||
        memcmp(output.value, wrap, output.length) != 0)
        errout("gss_unwrap_aead(aead2) decryption");
    free(fulltoken);
    (void)gss_release_buffer(&minor, &output);

    /* Wrap an AEAD token and unwrap it using a stream buffer. */
    wrap_aead(ctx1, sign, wrap, iov, data, conf);
    concat_iov(iov, 5, &fulltoken, &len);
    stiov[0].type = GSS_IOV_BUFFER_TYPE_STREAM;
    stiov[0].buffer.value = fulltoken;
    stiov[0].buffer.length = len;
    stiov[1].type = GSS_IOV_BUFFER_TYPE_SIGN_ONLY;
    stiov[1].buffer.value = (char *)sign;
    stiov[1].buffer.length = strlen(sign);
    stiov[2].type = GSS_IOV_BUFFER_TYPE_DATA;
    major = gss_unwrap_iov(&minor, ctx2, &oconf, &qop, stiov, 3);
    check_gsserr("gss_unwrap_iov(aead3)", major, minor);
    if (oconf != conf || qop != GSS_C_QOP_DEFAULT)
        errout("gss_unwrap_iov(aead3) conf/qop");
    if (stiov[2].buffer.length != strlen(wrap) ||
        memcmp(stiov[2].buffer.value, wrap, strlen(wrap)) != 0)
        errout("gss_unwrap_iov(aead3) decryption");
    offset = (char *)stiov[2].buffer.value - fulltoken;
    if (offset < 0 || (size_t)offset > len)
        errout("gss_unwrap_iov(aead3) offset");
    free(fulltoken);
    (void)gss_release_iov_buffer(&minor, iov, 4);

    /* Wrap a token using gss_wrap_aead and unwrap it using a stream buffer
     * with allocation and copying. */
    input.value = (char *)wrap;
    input.length = strlen(wrap);
    assoc.value = (char *)sign;
    assoc.length = strlen(sign);
    major = gss_wrap_aead(&minor, ctx1, conf, GSS_C_QOP_DEFAULT, &assoc,
                          &input, &oconf, &output);
    check_gsserr("gss_wrap_aead(aead4)", major, minor);
    if (oconf != conf)
        errout("gss_wrap(aead4) conf");
    stiov[0].type = GSS_IOV_BUFFER_TYPE_STREAM;
    stiov[0].buffer = output;
    stiov[1].type = GSS_IOV_BUFFER_TYPE_SIGN_ONLY;
    stiov[1].buffer = assoc;
    stiov[2].type = GSS_IOV_BUFFER_TYPE_DATA | GSS_IOV_BUFFER_FLAG_ALLOCATE;
    major = gss_unwrap_iov(&minor, ctx2, &oconf, &qop, stiov, 3);
    check_gsserr("gss_unwrap_iov(aead4)", major, minor);
    if (!(GSS_IOV_BUFFER_FLAGS(stiov[2].type) & GSS_IOV_BUFFER_FLAG_ALLOCATED))
        errout("gss_unwrap_iov(aead4) allocated");
    if (oconf != conf || qop != GSS_C_QOP_DEFAULT)
        errout("gss_unwrap_iov(aead4) conf/qop");
    if (stiov[2].buffer.length != strlen(wrap) ||
        memcmp(stiov[2].buffer.value, wrap, strlen(wrap)) != 0)
        errout("gss_unwrap_iov(aead4) decryption");
    (void)gss_release_buffer(&minor, &output);
    (void)gss_release_iov_buffer(&minor, stiov, 3);
}

/*
 * Get a MIC for sign1, sign2, and sign3 using the caller-provided array iov,
 * which must have space for four elements, and the caller-provided buffer
 * data, which must be big enough for the MIC.  If data is NULL, the library
 * will be asked to allocate the MIC buffer.  The MIC will be located in
 * iov[3].buffer.
 */
static void
mic(gss_ctx_id_t ctx, const char *sign1, const char *sign2, const char *sign3,
    gss_iov_buffer_desc *iov, char *data)
{
    OM_uint32 minor, major;
    krb5_boolean allocated;

    /* Lay out iov array. */
    iov[0].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[0].buffer.value = (char *)sign1;
    iov[0].buffer.length = strlen(sign1);
    iov[1].type = GSS_IOV_BUFFER_TYPE_SIGN_ONLY;
    iov[1].buffer.value = (char *)sign2;
    iov[1].buffer.length = strlen(sign2);
    iov[2].type = GSS_IOV_BUFFER_TYPE_SIGN_ONLY;
    iov[2].buffer.value = (char *)sign3;
    iov[2].buffer.length = strlen(sign3);
    iov[3].type = GSS_IOV_BUFFER_TYPE_MIC_TOKEN;
    if (data == NULL) {
        /* Ask the library to allocate the MIC buffer. */
        iov[3].type |= GSS_IOV_BUFFER_FLAG_ALLOCATE;
    } else {
        /* Get the MIC length and use the caller-provided buffer. */
        major = gss_get_mic_iov_length(&minor, ctx, GSS_C_QOP_DEFAULT, iov, 4);
        check_gsserr("gss_get_mic_iov_length", major, minor);
        iov[3].buffer.value = data;
    }
    major = gss_get_mic_iov(&minor, ctx, GSS_C_QOP_DEFAULT, iov, 4);
    check_gsserr("gss_get_mic_iov", major, minor);
    allocated = (GSS_IOV_BUFFER_FLAGS(iov[3].type) &
                 GSS_IOV_BUFFER_FLAG_ALLOCATED) != 0;
    if (allocated != (data == NULL))
        errout("gss_get_mic_iov allocated");
}

static void
test_mic(gss_ctx_id_t ctx1, gss_ctx_id_t ctx2)
{
    OM_uint32 major, minor;
    gss_iov_buffer_desc iov[4];
    gss_qop_t qop;
    gss_buffer_desc concatbuf, micbuf;
    const char *sign1 = "Data and sign-only ";
    const char *sign2 = "buffers are treated ";
    const char *sign3 = "equally by gss_get_mic_iov";
    char concat[1024], data[1024];

    (void)snprintf(concat, sizeof(concat), "%s%s%s", sign1, sign2, sign3);
    concatbuf.value = concat;
    concatbuf.length = strlen(concat);

    /* MIC with a caller-provided buffer and verify with the IOV array. */
    mic(ctx1, sign1, sign2, sign3, iov, data);
    major = gss_verify_mic_iov(&minor, ctx2, &qop, iov, 4);
    check_gsserr("gss_verify_mic_iov(mic1)", major, minor);
    if (qop != GSS_C_QOP_DEFAULT)
        errout("gss_verify_mic_iov(mic1) qop");

    /* MIC with an allocated buffer and verify with gss_verify_mic. */
    mic(ctx1, sign1, sign2, sign3, iov, NULL);
    major = gss_verify_mic(&minor, ctx2, &concatbuf, &iov[3].buffer, &qop);
    check_gsserr("gss_verify_mic(mic2)", major, minor);
    if (qop != GSS_C_QOP_DEFAULT)
        errout("gss_verify_mic(mic2) qop");
    (void)gss_release_iov_buffer(&minor, iov, 4);

    /* MIC with gss_c_get_mic and verify using the IOV array (which is still
     * mostly set up from the last call to mic(). */
    major = gss_get_mic(&minor, ctx1, GSS_C_QOP_DEFAULT, &concatbuf, &micbuf);
    check_gsserr("gss_get_mic(mic3)", major, minor);
    iov[3].buffer = micbuf;
    major = gss_verify_mic_iov(&minor, ctx2, &qop, iov, 4);
    check_gsserr("gss_verify_mic_iov(mic3)", major, minor);
    if (qop != GSS_C_QOP_DEFAULT)
        errout("gss_verify_mic_iov(mic3) qop");
    (void)gss_release_buffer(&minor, &micbuf);
}

/* Create a DCE-style token and make sure we can unwrap it. */
static void
test_dce(gss_ctx_id_t ctx1, gss_ctx_id_t ctx2, int conf)
{
    OM_uint32 major, minor;
    gss_iov_buffer_desc iov[4];
    gss_qop_t qop;
    const char *sign1 = "First data to be signed";
    const char *sign2 = "Second data to be signed";
    const char *wrap = "This data must align to 16 bytes";
    int oconf;
    char data[1024];

    /* Wrap a SIGN_ONLY_1 | DATA | SIGN_ONLY_2 | HEADER token. */
    memcpy(data, wrap, strlen(wrap) + 1);
    iov[0].type = GSS_IOV_BUFFER_TYPE_SIGN_ONLY;
    iov[0].buffer.value = (char *)sign1;
    iov[0].buffer.length = strlen(sign1);
    iov[1].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[1].buffer.value = data;
    iov[1].buffer.length = strlen(wrap);
    iov[2].type = GSS_IOV_BUFFER_TYPE_SIGN_ONLY;
    iov[2].buffer.value = (char *)sign2;
    iov[2].buffer.length = strlen(sign2);
    iov[3].type = GSS_IOV_BUFFER_TYPE_HEADER | GSS_IOV_BUFFER_FLAG_ALLOCATE;
    major = gss_wrap_iov(&minor, ctx1, conf, GSS_C_QOP_DEFAULT, &oconf, iov,
                         4);
    check_gsserr("gss_wrap_iov(dce)", major, minor);
    if (oconf != conf)
        errout("gss_wrap_iov(dce) conf");
    if (iov[0].buffer.value != sign1 || iov[0].buffer.length != strlen(sign1))
        errout("gss_wrap_iov(dce) sign1 buffer");
    if (iov[1].buffer.value != data || iov[1].buffer.length != strlen(wrap))
        errout("gss_wrap_iov(dce) data buffer");
    if (iov[2].buffer.value != sign2 || iov[2].buffer.length != strlen(sign2))
        errout("gss_wrap_iov(dce) sign2 buffer");
    check_encrypted("gss_wrap_iov(dce) encryption", conf, data, wrap);

    /* Make sure we can unwrap it. */
    major = gss_unwrap_iov(&minor, ctx2, &oconf, &qop, iov, 4);
    check_gsserr("gss_unwrap_iov(std1)", major, minor);
    if (oconf != conf || qop != GSS_C_QOP_DEFAULT)
        errout("gss_unwrap_iov(std1) conf/qop");
    if (iov[0].buffer.value != sign1 || iov[0].buffer.length != strlen(sign1))
        errout("gss_unwrap_iov(dce) sign1 buffer");
    if (iov[1].buffer.value != data || iov[1].buffer.length != strlen(wrap))
        errout("gss_unwrap_iov(dce) data buffer");
    if (iov[2].buffer.value != sign2 || iov[2].buffer.length != strlen(sign2))
        errout("gss_unwrap_iov(dce) sign2 buffer");
    if (memcmp(data, wrap, iov[1].buffer.length) != 0)
        errout("gss_unwrap_iov(dce) decryption");
    (void)gss_release_iov_buffer(&minor, iov, 4);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, flags;
    gss_OID mech = &mech_krb5;
    gss_name_t tname;
    gss_ctx_id_t ictx, actx;

    /* Parse arguments. */
    argv++;
    if (*argv != NULL && strcmp(*argv, "-s") == 0) {
        mech = &mech_spnego;
        argv++;
    }
    if (*argv == NULL || *(argv + 1) != NULL)
        errout("Usage: t_iov [-s] targetname");
    tname = import_name(*argv);

    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_MUTUAL_FLAG;
    establish_contexts(mech, GSS_C_NO_CREDENTIAL, GSS_C_NO_CREDENTIAL, tname,
                       flags, &ictx, &actx, NULL, NULL, NULL);

    /* Test standard token wrapping and unwrapping in both directions, with and
     * without confidentiality. */
    test_standard_wrap(ictx, actx, 0);
    test_standard_wrap(ictx, actx, 1);
    test_standard_wrap(actx, ictx, 0);
    test_standard_wrap(actx, ictx, 1);

    /* Test AEAD wrapping. */
    test_aead(ictx, actx, 0);
    test_aead(ictx, actx, 1);
    test_aead(actx, ictx, 0);
    test_aead(actx, ictx, 1);

    /* Test MIC tokens. */
    test_mic(ictx, actx);
    test_mic(actx, ictx);

    /* Test DCE wrapping with DCE-style contexts. */
    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_DCE_STYLE;
    establish_contexts(mech, GSS_C_NO_CREDENTIAL, GSS_C_NO_CREDENTIAL, tname,
                       flags, &ictx, &actx, NULL, NULL, NULL);
    test_dce(ictx, actx, 0);
    test_dce(ictx, actx, 1);
    test_dce(actx, ictx, 0);
    test_dce(actx, ictx, 1);

    (void)gss_release_name(&minor, &tname);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
    return 0;
}
