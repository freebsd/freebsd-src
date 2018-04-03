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

#include "k5-platform.h"
#include <krb5.h>

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
    krb5_get_init_creds_opt *opt;
    krb5_creds creds;
    krb5_boolean stepwise = FALSE;
    krb5_preauthtype ptypes[64];
    int c, nptypes = 0;
    char *val;

    check(krb5_init_context(&ctx));
    check(krb5_get_init_creds_opt_alloc(ctx, &opt));

    while ((c = getopt(argc, argv, "so:X:")) != -1) {
        switch (c) {
        case 's':
            stepwise = TRUE;
            break;
        case 'o':
            assert(nptypes < 64);
            ptypes[nptypes++] = atoi(optarg);
            break;
        case 'X':
            val = strchr(optarg, '=');
            if (val != NULL)
                *val++ = '\0';
            else
                val = "yes";
            check(krb5_get_init_creds_opt_set_pa(ctx, opt, optarg, val));
            break;
        default:
            abort();
        }
    }

    argc -= optind;
    argv += optind;
    if (argc != 2)
        abort();
    princstr = argv[0];
    password = argv[1];

    check(krb5_parse_name(ctx, princstr, &client));

    if (nptypes > 0)
        krb5_get_init_creds_opt_set_preauth_list(opt, ptypes, nptypes);

    if (stepwise) {
        /* Use the stepwise interface. */
        check(krb5_init_creds_init(ctx, client, NULL, NULL, 0, NULL, &icc));
        check(krb5_init_creds_set_password(ctx, icc, password));
        check(krb5_init_creds_get(ctx, icc));
        krb5_init_creds_free(ctx, icc);
    } else {
        /* Use the traditional one-shot interface. */
        check(krb5_get_init_creds_password(ctx, &creds, client, password, NULL,
                                           NULL, 0, NULL, opt));
        krb5_free_cred_contents(ctx, &creds);
    }

    krb5_get_init_creds_opt_free(ctx, opt);
    krb5_free_principal(ctx, client);
    krb5_free_context(ctx);
    return 0;
}
