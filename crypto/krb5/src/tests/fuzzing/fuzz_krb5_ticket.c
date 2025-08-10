/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/fuzz_krb5_ticket.c */
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
 * Fuzzing harness implementation for krb5_decode_ticket.
 */

#include "autoconf.h"
#include <k5-int.h>
#include <krb5.h>
#include <string.h>

#define kMinInputLength 2
#define kMaxInputLength 2048

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    krb5_error_code ret;
    krb5_context context = NULL;
    krb5_keytab defkt = NULL;
    krb5_data data_in, *data_out;
    krb5_ticket *ticket = NULL;

    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    data_in = make_data((void *)data, size);

    ret = krb5_init_context(&context);
    if (ret)
        return 0;

    ret = krb5_kt_default(context, &defkt);
    if (ret)
        goto cleanup;

    ret = krb5_decode_ticket(&data_in, &ticket);
    if (ret)
        goto cleanup;

    ret = encode_krb5_ticket(ticket, &data_out);
    if (!ret)
        krb5_free_data(context, data_out);

    krb5_server_decrypt_ticket_keytab(context, defkt, ticket);

cleanup:
    krb5_free_ticket(context, ticket);
    if (defkt != NULL)
        krb5_kt_close(context, defkt);
    krb5_free_context(context);

    return 0;
}
