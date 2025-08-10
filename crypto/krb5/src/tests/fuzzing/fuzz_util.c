/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/fuzz_util.c */
/*
 * Copyright (C) 2024 by Arjun. All rights reserved.
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
 * Fuzzing harness implementation for k5_base64_decode, k5_hex_decode
 * krb5_parse_name and k5_parse_host_string.
 */

#include "autoconf.h"
#include <k5-int.h>
#include <k5-base64.h>
#include <k5-hex.h>
#include <string.h>
#include <k5-utf8.h>

#include <hashtab.c>

#define kMinInputLength 2
#define kMaxInputLength 256

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void
fuzz_base64(const char *data_in, size_t size)
{
    size_t len;

    free(k5_base64_encode(data_in, size));
    free(k5_base64_decode(data_in, &len));
}

static void
fuzz_hashtab(const char *data_in, size_t size)
{
    int st;
    struct k5_hashtab *ht;

    k5_hashtab_create(NULL, 4, &ht);
    if (ht == NULL)
        return;

    k5_hashtab_add(ht, data_in, size, &st);

    k5_hashtab_free(ht);
}

static void
fuzz_hex(const char *data_in, size_t size)
{
    char *hex;
    uint8_t *bytes;
    size_t len;

    if (k5_hex_encode(data_in, size, 0, &hex) == 0)
        free(hex);

    if (k5_hex_encode(data_in, size, 1, &hex) == 0)
        free(hex);

    if (k5_hex_decode(data_in, &bytes, &len) == 0)
        free(bytes);
}

static void
fuzz_name(const char *data_in, size_t size)
{
    krb5_context context;
    krb5_principal fuzzing;

    if (krb5_init_context(&context) != 0)
        return;

    krb5_parse_name(context, data_in, &fuzzing);

    krb5_free_principal(context, fuzzing);
    krb5_free_context(context);
}

static void
fuzz_parse_host(const char *data_in, size_t size)
{
    char *host_out = NULL;
    int port_out = -1;

    if (k5_parse_host_string(data_in, 1, &host_out, &port_out) == 0)
        free(host_out);
}

static void
fuzz_utf8(const char *data_in, size_t size)
{
    krb5_ucs4 u = 0;
    char *utf8;
    uint8_t *utf16;
    size_t utf16len;

    krb5int_utf8_to_ucs4(data_in, &u);

    k5_utf8_to_utf16le(data_in, &utf16, &utf16len);
    if (utf16 != NULL)
        free(utf16);

    k5_utf16le_to_utf8((const uint8_t *)data_in, size, &utf8);
    if (utf8 != NULL)
        free(utf8);
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    krb5_error_code ret;
    char *data_in;

    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    data_in = k5memdup0(data, size, &ret);
    if (data_in == NULL)
        return 0;

    fuzz_base64(data_in, size);
    fuzz_hashtab(data_in, size);
    fuzz_hex(data_in, size);
    fuzz_name(data_in, size);
    fuzz_parse_host(data_in, size);
    fuzz_utf8(data_in, size);

    free(data_in);

    return 0;
}
