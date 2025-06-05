/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* t_in_ccache.c: get creds while using input and/or armor ccaches */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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
 * A test helper that exercises the input-ccache option, potentially in
 * combination with armor-ccache options.
 */

#include "k5-int.h"

static void
bail_on_err(krb5_context context, const char *msg, krb5_error_code code)
{
    const char *errmsg;

    if (code) {
        errmsg = krb5_get_error_message(context, code);
        printf("%s: %s\n", msg, errmsg);
        krb5_free_error_message(context, errmsg);
        exit(1);
    }
}

static krb5_error_code
prompter_cb(krb5_context ctx, void *data, const char *name,
            const char *banner, int num_prompts, krb5_prompt prompts[])
{
    /* Not expecting any actual prompts. */
    if (num_prompts != 0) {
        printf("too many prompts passed to prompter callback (%d), failing\n",
               num_prompts);
        exit(1);
    }
    return 0;
}

int
main(int argc, char **argv)
{
    krb5_context ctx;
    krb5_ccache in_ccache, out_ccache, armor_ccache;
    krb5_get_init_creds_opt *opt;
    char *user, *password, *armor_ccname = NULL, *in_ccname = NULL, *perr;
    const char *err;
    krb5_principal client;
    krb5_creds creds;
    krb5_flags fast_flags;
    krb5_error_code ret;
    int c;

    while ((c = getopt(argc, argv, "I:A:")) != -1) {
        switch (c) {
        case 'A':
            armor_ccname = optarg;
            break;
        case 'I':
            in_ccname = optarg;
            break;
        }
    }
    if (argc - optind < 2) {
        fprintf(stderr, "Usage: %s [-A armor_ccache] [-I in_ccache] "
                "username password\n", argv[0]);
        return 1;
    }
    user = argv[optind];
    password = argv[optind + 1];

    bail_on_err(NULL, "Error initializing Kerberos", krb5_init_context(&ctx));
    bail_on_err(ctx, "Error allocating space for get_init_creds options",
                krb5_get_init_creds_opt_alloc(ctx, &opt));
    if (in_ccname != NULL) {
        bail_on_err(ctx, "Error resolving input ccache",
                    krb5_cc_resolve(ctx, in_ccname, &in_ccache));
        bail_on_err(ctx, "Error setting input_ccache option",
                    krb5_get_init_creds_opt_set_in_ccache(ctx, opt,
                                                          in_ccache));
    } else {
        in_ccache = NULL;
    }
    if (armor_ccname != NULL) {
        bail_on_err(ctx, "Error resolving armor ccache",
                    krb5_cc_resolve(ctx, armor_ccname, &armor_ccache));
        bail_on_err(ctx, "Error setting fast_ccache option",
                    krb5_get_init_creds_opt_set_fast_ccache(ctx, opt,
                                                            armor_ccache));
        fast_flags = KRB5_FAST_REQUIRED;
        bail_on_err(ctx, "Error setting option to force use of FAST",
                    krb5_get_init_creds_opt_set_fast_flags(ctx, opt,
                                                           fast_flags));
    } else {
        armor_ccache = NULL;
    }
    bail_on_err(ctx, "Error resolving output (default) ccache",
                krb5_cc_default(ctx, &out_ccache));
    bail_on_err(ctx, "Error setting output ccache option",
                krb5_get_init_creds_opt_set_out_ccache(ctx, opt, out_ccache));
    if (asprintf(&perr, "Error parsing principal name \"%s\"", user) < 0)
        abort();
    bail_on_err(ctx, perr, krb5_parse_name(ctx, user, &client));
    ret = krb5_get_init_creds_password(ctx, &creds, client, password,
                                       prompter_cb, NULL, 0, NULL, opt);
    if (ret) {
        err = krb5_get_error_message(ctx, ret);
        printf("%s\n", err);
        krb5_free_error_message(ctx, err);
    } else {
        krb5_free_cred_contents(ctx, &creds);
    }
    krb5_get_init_creds_opt_free(ctx, opt);
    krb5_free_principal(ctx, client);
    krb5_cc_close(ctx, out_ccache);
    if (armor_ccache != NULL)
        krb5_cc_close(ctx, armor_ccache);
    if (in_ccache != NULL)
        krb5_cc_close(ctx, in_ccache);
    krb5_free_context(ctx);
    free(perr);
    return ret ? (ret - KRB5KDC_ERR_NONE) : 0;
}
