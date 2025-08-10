/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/fuzz_gss.c */
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
 * Fuzzing harness implementation for gss_accept_sec_context.
 */

#include "autoconf.h"
#include <krb5.h>
#include <gssapi.h>
#include <string.h>

#define kMinInputLength 2
#define kMaxInputLength 1024

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    gss_OID doid;
    OM_uint32 minor, ret_flags, time_rec;
    gss_name_t client = GSS_C_NO_NAME;
    gss_ctx_id_t context = GSS_C_NO_CONTEXT;
    gss_cred_id_t deleg_cred = GSS_C_NO_CREDENTIAL;
    gss_buffer_desc data_in, data_out = GSS_C_EMPTY_BUFFER;

    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    data_in.length = size;
    data_in.value = (void *)data;

    gss_accept_sec_context(&minor, &context, GSS_C_NO_CREDENTIAL,
                           &data_in, GSS_C_NO_CHANNEL_BINDINGS, &client,
                           &doid, &data_out, &ret_flags, &time_rec,
                           &deleg_cred);

    gss_release_buffer(&minor, &data_out);

    if (context != GSS_C_NO_CONTEXT)
        gss_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);

    return 0;
}
