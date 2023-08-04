/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gcred.c - Test harness for referrals */
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
 * This program is intended to be run from a python script as:
 *
 *     gcred [-f] [-t] nametype princname
 *
 * where nametype is one of "unknown", "principal", "srv-inst", and "srv-hst",
 * and princname is the name of the service principal.  gcred acquires
 * credentials for the specified server principal.  On success, gcred displays
 * the server principal name of the obtained credentials to stdout and exits
 * with status 0.  On failure, gcred displays the error message for the failed
 * operation to stderr and exits with status 1.
 *
 * The -f and -t flags set the KRB5_GC_FORWARDABLE and KRB5_GC_NO_TRANSIT_CHECK
 * options respectively.
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
    krb5_principal client, server;
    krb5_ccache ccache;
    krb5_creds in_creds, *creds;
    krb5_ticket *ticket;
    krb5_flags options = 0;
    char *name;
    int c;

    check(krb5_init_context(&ctx));

    while ((c = getopt(argc, argv, "ft")) != -1) {
        switch (c) {
        case 'f':
            options |= KRB5_GC_FORWARDABLE;
            break;
        case 't':
            options |= KRB5_GC_NO_TRANSIT_CHECK;
            break;
        default:
            abort();
        }
    }
    argc -= optind;
    argv += optind;
    assert(argc == 2);
    check(krb5_parse_name(ctx, argv[1], &server));
    if (strcmp(argv[0], "unknown") == 0)
        server->type = KRB5_NT_UNKNOWN;
    else if (strcmp(argv[0], "principal") == 0)
        server->type = KRB5_NT_PRINCIPAL;
    else if (strcmp(argv[0], "srv-inst") == 0)
        server->type = KRB5_NT_SRV_INST;
    else if (strcmp(argv[0], "srv-hst") == 0)
        server->type = KRB5_NT_SRV_HST;
    else
        abort();

    check(krb5_cc_default(ctx, &ccache));
    check(krb5_cc_get_principal(ctx, ccache, &client));
    memset(&in_creds, 0, sizeof(in_creds));
    in_creds.client = client;
    in_creds.server = server;
    check(krb5_get_credentials(ctx, options, ccache, &in_creds, &creds));
    check(krb5_decode_ticket(&creds->ticket, &ticket));
    check(krb5_unparse_name(ctx, ticket->server, &name));
    printf("%s\n", name);

    krb5_free_ticket(ctx, ticket);
    krb5_free_unparsed_name(ctx, name);
    krb5_free_creds(ctx, creds);
    krb5_free_principal(ctx, client);
    krb5_free_principal(ctx, server);
    krb5_cc_close(ctx, ccache);
    krb5_free_context(ctx);
    return 0;
}
