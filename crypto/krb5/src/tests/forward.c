/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/forward.c - test harness for getting forwarded creds */
/*
 * Copyright (C) 2016 by the Massachusetts Institute of Technology.
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

/* This test program overwrites the default credential cache with a forwarded
 * TGT obtained using the TGT presently in the cache. */

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
main()
{
    krb5_ccache cc;
    krb5_creds mcred, tgt, *fcred;
    krb5_principal client, tgtprinc;
    krb5_flags options;

    /* Open the default ccache and get the client and TGT principal names. */
    check(krb5_init_context(&ctx));
    check(krb5_cc_default(ctx, &cc));
    check(krb5_cc_get_principal(ctx, cc, &client));
    check(krb5_build_principal_ext(ctx, &tgtprinc, client->realm.length,
                                   client->realm.data, KRB5_TGS_NAME_SIZE,
                                   KRB5_TGS_NAME, client->realm.length,
                                   client->realm.data, 0));

    /* Fetch the TGT credential. */
    memset(&mcred, 0, sizeof(mcred));
    mcred.client = client;
    mcred.server = tgtprinc;
    check(krb5_cc_retrieve_cred(ctx, cc, 0, &mcred, &tgt));

    /* Get a forwarded TGT. */
    mcred.times = tgt.times;
    mcred.times.starttime = 0;
    options = (tgt.ticket_flags & KDC_TKT_COMMON_MASK) | KDC_OPT_FORWARDED;
    check(krb5_get_cred_via_tkt(ctx, &tgt, options, NULL, &mcred, &fcred));

    /* Reinitialize the default ccache with the forwarded TGT. */
    check(krb5_cc_initialize(ctx, cc, client));
    check(krb5_cc_store_cred(ctx, cc, fcred));

    krb5_free_creds(ctx, fcred);
    krb5_free_cred_contents(ctx, &tgt);
    krb5_free_principal(ctx, tgtprinc);
    krb5_free_principal(ctx, client);
    krb5_cc_close(ctx, cc);
    krb5_free_context(ctx);
    return 0;
}
