/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_expire_warn.c - Test harness for password expiry warnings */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"

static int exp_dummy, prompt_dummy;

static void
check(krb5_error_code code)
{
    if (code != 0)
        abort();
}

static krb5_error_code
prompter_cb(krb5_context ctx, void *data, const char *name,
            const char *banner, int num_prompts, krb5_prompt prompts[])
{
    /* Not expecting any actual prompts, only banners. */
    assert(num_prompts == 0);
    assert(banner != NULL);
    printf("Prompter: %s\n", banner);
    return 0;
}

static void
expire_cb(krb5_context ctx, void *data, krb5_timestamp password_expiration,
          krb5_timestamp account_expiration, krb5_boolean is_last_req)
{
    printf("password_expiration = %ld\n", (long)password_expiration);
    printf("account_expiration = %ld\n", (long)account_expiration);
    printf("is_last_req = %d\n", (int)is_last_req);
}

int
main(int argc, char **argv)
{
    krb5_context ctx;
    krb5_init_creds_context icctx;
    krb5_get_init_creds_opt *opt;
    char *user, *password, *service = NULL;
    krb5_boolean use_cb, stepwise;
    krb5_principal client;
    krb5_creds creds;

    if (argc < 5) {
        fprintf(stderr, "Usage: %s username password {1|0} {1|0} [service]\n",
                argv[0]);
        return 1;
    }
    user = argv[1];
    password = argv[2];
    use_cb = atoi(argv[3]);
    stepwise = atoi(argv[4]);
    if (argc >= 6)
        service = argv[5];

    check(krb5_init_context(&ctx));
    check(krb5_get_init_creds_opt_alloc(ctx, &opt));
    if (use_cb) {
        check(krb5_get_init_creds_opt_set_expire_callback(ctx, opt, expire_cb,
                                                          &exp_dummy));
    }
    check(krb5_parse_name(ctx, user, &client));
    if (stepwise) {
        check(krb5_init_creds_init(ctx, client, prompter_cb, &prompt_dummy, 0,
                                   opt, &icctx));
        krb5_init_creds_set_password(ctx, icctx, password);
        if (service != NULL)
            check(krb5_init_creds_set_service(ctx, icctx, service));
        check(krb5_init_creds_get(ctx, icctx));
        krb5_init_creds_free(ctx, icctx);
    } else {
        check(krb5_get_init_creds_password(ctx, &creds, client, password,
                                           prompter_cb, &prompt_dummy, 0,
                                           service, opt));
        krb5_free_cred_contents(ctx, &creds);
    }
    krb5_get_init_creds_opt_free(ctx, opt);
    krb5_free_principal(ctx, client);
    krb5_free_context(ctx);
    return 0;
}
