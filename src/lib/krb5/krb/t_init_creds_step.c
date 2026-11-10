/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_init_creds_step.c - test krb5_init_creds_step() replies */
/*
 * Copyright (C) 2026 by Alejandro Pacheco.
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
 * Feed short or unrecognized KDC replies to krb5_init_creds_step() and check
 * that they are rejected with the expected error code.
 */

#include "k5-int.h"

static void
check_reply(krb5_context context, krb5_principal client,
            const char *bytes, size_t len, krb5_error_code expected)
{
    krb5_error_code ret;
    krb5_init_creds_context icc;
    krb5_data reply, request, realm;
    unsigned int flags;

    ret = krb5_init_creds_init(context, client, NULL, NULL, 0, NULL, &icc);
    assert(!ret);

    reply = empty_data();
    ret = krb5_init_creds_step(context, icc, &reply, &request, &realm, &flags);
    assert(!ret);
    assert(flags == KRB5_INIT_CREDS_STEP_FLAG_CONTINUE);
    krb5_free_data_contents(context, &request);
    krb5_free_data_contents(context, &realm);

    /* Allocate exactly len bytes, as a KDC reply buffer would be. */
    reply.data = malloc(len);
    assert(reply.data != NULL);
    memcpy(reply.data, bytes, len);
    reply.length = len;
    ret = krb5_init_creds_step(context, icc, &reply, &request, &realm, &flags);
    assert(ret == expected);

    free(reply.data);
    krb5_free_data_contents(context, &request);
    krb5_free_data_contents(context, &realm);
    krb5_init_creds_free(context, icc);
}

int
main(void)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_principal client;

    ret = krb5_init_context(&context);
    assert(!ret);
    ret = krb5_parse_name(context, "user@KRBTEST.COM", &client);
    assert(!ret);

    /* A one-byte reply is too short to be a V4 error reply. */
    check_reply(context, client, "\x04", 1, KRB5KRB_AP_ERR_MSG_TYPE);

    /* A V4 error reply is recognized as such. */
    check_reply(context, client, "\x04\x0a", 2, KRB5KRB_AP_ERR_V4_REPLY);

    /* Other non-AS-REP replies are rejected as the wrong message type. */
    check_reply(context, client, "\x05\x0a", 2, KRB5KRB_AP_ERR_MSG_TYPE);

    krb5_free_principal(context, client);
    krb5_free_context(context);
    return 0;
}
