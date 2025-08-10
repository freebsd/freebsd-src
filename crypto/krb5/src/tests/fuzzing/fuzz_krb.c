/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/krb.c - fuzzing harness for miscellaneous libkrb5 functions */
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

#include "autoconf.h"
#include <k5-int.h>

#define kMinInputLength 2
#define kMaxInputLength 512

#define ANAME_SZ 40
#define INST_SZ  40
#define REALM_SZ  40

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void
fuzz_deltat(char *data_in)
{
    krb5_deltat result;
    krb5_string_to_deltat(data_in, &result);
}

static void
fuzz_host_string(char *data_in)
{
    krb5_error_code ret;
    char *host;
    int port = -1;

    ret = k5_parse_host_string(data_in, 0, &host, &port);
    if (!ret)
        free(host);
}

static void
fuzz_princ(krb5_context context, char *data_in)
{
    krb5_error_code ret;
    krb5_principal p;
    char *princ;

    ret = krb5_parse_name(context, data_in, &p);
    if (ret)
        return;

    ret = krb5_unparse_name(context, p, &princ);
    if (!ret)
        free(princ);

    krb5_free_principal(context, p);
}

static void
fuzz_principal_425(krb5_context context, char *data_in)
{
    krb5_principal princ;
    krb5_425_conv_principal(context, data_in, data_in, data_in, &princ);
    krb5_free_principal(context, princ);
}

static void
fuzz_principal_524(krb5_context context, char *data_in)
{
    krb5_error_code ret;
    krb5_principal princ = 0;
    char aname[ANAME_SZ + 1], inst[INST_SZ + 1], realm[REALM_SZ + 1];

    aname[ANAME_SZ] = inst[INST_SZ] = realm[REALM_SZ] = 0;

    ret = krb5_parse_name(context, data_in, &princ);
    if (ret)
        return;

    krb5_524_conv_principal(context, princ, aname, inst, realm);
    krb5_free_principal(context, princ);
}

static void
fuzz_timestamp(char *data_in)
{
    krb5_error_code ret;
    krb5_timestamp timestamp;

    ret = krb5_string_to_timestamp(data_in, &timestamp);
    if (!ret)
        ts2tt(timestamp);
}

/*
 * data_in is going to be modified during parsing.
 */
static void
fuzz_enctype_list(char *data_in)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_enctype *ienc, zero = 0;

    ret = krb5_init_context(&context);
    if (ret)
        return;

    ret = krb5int_parse_enctype_list(context, "", data_in, &zero, &ienc);
    if (!ret)
        free(ienc);

    krb5_free_context(context);
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    krb5_error_code ret;
    krb5_context context = NULL;
    char *data_in;

    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    ret = krb5_init_context(&context);
    if (ret)
        return 0;

    data_in = k5memdup0(data, size, &ret);
    if (ret)
        goto cleanup;

    fuzz_deltat(data_in);
    fuzz_host_string(data_in);
    fuzz_princ(context, data_in);
    fuzz_principal_425(context, data_in);
    fuzz_principal_524(context, data_in);
    fuzz_timestamp(data_in);
    fuzz_enctype_list(data_in);

    free(data_in);

cleanup:
    krb5_free_context(context);

    return 0;
}
