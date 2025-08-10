/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/fuzz_pac.c */
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
 * Fuzzing harness implementation for krb5_pac_parse.
 */

#include "autoconf.h"
#include <k5-int.h>

#define U(x) (uint8_t *)x
#define kMinInputLength 2
#define kMaxInputLength 1024

static const krb5_keyblock kdc_keyblock = {
    0, ENCTYPE_ARCFOUR_HMAC,
    16, U("\xB2\x86\x75\x71\x48\xAF\x7F\xD2\x52\xC5\x36\x03\xA1\x50\xB7\xE7")
};

static const krb5_keyblock member_keyblock = {
    0, ENCTYPE_ARCFOUR_HMAC,
    16, U("\xD2\x17\xFA\xEA\xE5\xE6\xB5\xF9\x5C\xCC\x94\x07\x7A\xB8\xA5\xFC")
};

static time_t authtime = 1120440609;
static const char *user = "w2003final$@WIN2K3.THINKER.LOCAL";

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    krb5_error_code ret;
    krb5_context context = NULL;
    krb5_pac pac;
    krb5_principal princ = NULL;

    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    ret = krb5_init_context(&context);
    if (ret)
        return 0;

    ret = krb5_parse_name(context, user, &princ);
    if (ret)
        goto cleanup;

    ret = krb5_pac_parse(context, data, size, &pac);
    if (ret)
        goto cleanup;

    krb5_pac_verify(context, pac, authtime, princ, NULL, NULL);
    krb5_pac_verify_ext(context, pac, authtime, princ, NULL, NULL, TRUE);
    krb5_pac_verify(context, pac, authtime, princ, &member_keyblock,
                    &kdc_keyblock);

    krb5_pac_free(context, pac);

cleanup:
    krb5_free_principal(context, princ);
    krb5_free_context(context);

    return 0;
}
