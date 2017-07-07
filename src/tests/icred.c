/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/icred.c - test harness for getting initial creds */
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

/*
 * This program exercises the init_creds APIs in ways kinit doesn't.  Right now
 * it is very simplistic, but it can be extended as needed.
 */

#include <krb5.h>
#include <stdio.h>

static krb5_context ctx;

static void
check(krb5_error_code code)
{
    const char *errmsg;

    if (code) {
        errmsg = krb5_get_error_message(ctx, code);
        fprintf(stderr, "%s\n", errmsg);
        krb5_free_error_message(ctx, errmsg);
        exit(1);
    }
}

int
main(int argc, char **argv)
{
    const char *princstr, *password;
    krb5_principal client;
    krb5_init_creds_context icc;
    krb5_creds creds;

    if (argc != 3) {
        fprintf(stderr, "Usage: icred princname password\n");
        exit(1);
    }
    princstr = argv[1];
    password = argv[2];

    check(krb5_init_context(&ctx));
    check(krb5_parse_name(ctx, princstr, &client));

    /* Try once with the traditional interface. */
    check(krb5_get_init_creds_password(ctx, &creds, client, password, NULL,
                                       NULL, 0, NULL, NULL));
    krb5_free_cred_contents(ctx, &creds);

    /* Try again with the step interface. */
    check(krb5_init_creds_init(ctx, client, NULL, NULL, 0, NULL, &icc));
    check(krb5_init_creds_set_password(ctx, icc, password));
    check(krb5_init_creds_get(ctx, icc));
    krb5_init_creds_free(ctx, icc);

    krb5_free_principal(ctx, client);
    krb5_free_context(ctx);
    return 0;
}
