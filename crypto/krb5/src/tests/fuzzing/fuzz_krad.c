/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/fuzz_krad.c */
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
 * Fuzzing harness implementation for krad_packet_decode_response,
 * krad_packet_decode_request.
 */

#include "autoconf.h"
#include <k5-int.h>
#include <krad.h>

#define kMinInputLength 2
#define kMaxInputLength 1024

static krad_packet *packets[3];

static const krad_packet *
iterator(void *data, krb5_boolean cancel)
{
    krad_packet *tmp;
    int *i = data;

    if (cancel || packets[*i] == NULL)
        return NULL;

    tmp = packets[*i];
    *i += 1;
    return tmp;
}

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    int i;
    krb5_context ctx;
    krb5_data data_in;
    const char *secret = "f";
    const krad_packet *req_1 = NULL, *req_2 = NULL;
    krad_packet *rsp_1 = NULL, *rsp_2 = NULL;

    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    if (krb5_init_context(&ctx) != 0)
        return 0;

    data_in = make_data((void *)data, size);

    i = 0;
    krad_packet_decode_response(ctx, secret, &data_in, iterator, &i,
                                &req_1, &rsp_1);

    i = 0;
    krad_packet_decode_request(ctx, secret, &data_in, iterator, &i,
                               &req_2, &rsp_2);

    krad_packet_free(rsp_1);
    krad_packet_free(rsp_2);
    krb5_free_context(ctx);

    return 0;
}
