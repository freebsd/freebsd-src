/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/icinterleave.c - interleaved init_creds_step test harness */
/*
 * Copyright (C) 2017 by the Massachusetts Institute of Technology.
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
 * This test harness performs multiple initial creds operations using
 * krb5_init_creds_step(), interleaving the operations to test the scoping of
 * the preauth state.  All principals must have the same password (or not
 * require a password).
 */

#include "k5-int.h"

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
    const char *password;
    char **princstrs;
    krb5_principal client;
    krb5_init_creds_context *iccs;
    krb5_data req, *reps, realm;
    krb5_boolean any_left;
    int i, nclients, master;
    unsigned int flags;

    if (argc < 3) {
        fprintf(stderr, "Usage: icinterleave password princ1 princ2 ...\n");
        exit(1);
    }
    password = argv[1];
    princstrs = argv + 2;
    nclients = argc - 2;

    check(krb5_init_context(&ctx));

    /* Create an initial creds context for each client principal. */
    iccs = calloc(nclients, sizeof(*iccs));
    assert(iccs != NULL);
    for (i = 0; i < nclients; i++) {
        check(krb5_parse_name(ctx, princstrs[i], &client));
        check(krb5_init_creds_init(ctx, client, NULL, NULL, 0, NULL,
                                   &iccs[i]));
        check(krb5_init_creds_set_password(ctx, iccs[i], password));
        krb5_free_principal(ctx, client);
    }

    reps = calloc(nclients, sizeof(*reps));
    assert(reps != NULL);

    any_left = TRUE;
    while (any_left) {
        any_left = FALSE;
        for (i = 0; i < nclients; i++)  {
            if (iccs[i] == NULL)
                continue;
            any_left = TRUE;

            printf("step %d\n", i + 1);

            req = empty_data();
            realm = empty_data();
            check(krb5_init_creds_step(ctx, iccs[i], &reps[i], &req, &realm,
                                       &flags));
            if (!(flags & KRB5_INIT_CREDS_STEP_FLAG_CONTINUE)) {
                printf("finish %d\n", i + 1);
                krb5_init_creds_free(ctx, iccs[i]);
                iccs[i] = NULL;
                continue;
            }

            master = 0;
            krb5_free_data_contents(ctx, &reps[i]);
            check(krb5_sendto_kdc(ctx, &req, &realm, &reps[i], &master, 0));
            krb5_free_data_contents(ctx, &req);
            krb5_free_data_contents(ctx, &realm);
        }
    }

    for (i = 0; i < nclients; i++)
        krb5_free_data_contents(ctx, &reps[i]);
    free(reps);
    free(iccs);
    krb5_free_context(ctx);
    return 0;
}
