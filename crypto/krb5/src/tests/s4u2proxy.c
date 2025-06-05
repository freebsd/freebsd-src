/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/s4u2proxy.c - S4U2Proxy test harness */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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
 * Usage: s4u2proxy evccname targetname [ad-type ad-contents]
 *
 * evccname contains an evidence ticket.  The default ccache contains a TGT for
 * the intermediate service.  The default keytab contains a key for the
 * intermediate service.  An S4U2Proxy request is made to get a ticket from
 * evccname's default principal to the target service.  The resulting cred is
 * stored in the default ccache.
 */

#include <k5-int.h>

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

static krb5_authdata **
make_request_authdata(int type, const char *contents)
{
    krb5_authdata *ad;
    krb5_authdata **req_authdata;

    ad = malloc(sizeof(*ad));
    assert(ad != NULL);
    ad->magic = KV5M_AUTHDATA;
    ad->ad_type = type;
    ad->length = strlen(contents);
    ad->contents = (unsigned char *)strdup(contents);
    assert(ad->contents != NULL);

    req_authdata = malloc(2 * sizeof(*req_authdata));
    assert(req_authdata != NULL);
    req_authdata[0] = ad;
    req_authdata[1] = NULL;

    return req_authdata;
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_ccache defcc, evcc;
    krb5_principal client_name, int_name, target_name;
    krb5_keytab defkt;
    krb5_creds mcred, ev_cred, *new_cred;
    krb5_ticket *ev_ticket;
    krb5_authdata **req_authdata = NULL;

    if (argc == 5) {
        req_authdata = make_request_authdata(atoi(argv[3]), argv[4]);
        argc -= 2;
    }

    assert(argc == 3);
    check(krb5_init_context(&context));

    /* Open the default ccache, evidence ticket ccache, and default keytab. */
    check(krb5_cc_default(context, &defcc));
    check(krb5_cc_resolve(context, argv[1], &evcc));
    check(krb5_kt_default(context, &defkt));

    /* Determine the client name, intermediate name, and target name. */
    check(krb5_cc_get_principal(context, evcc, &client_name));
    check(krb5_cc_get_principal(context, defcc, &int_name));
    check(krb5_parse_name(context, argv[2], &target_name));

    /* Retrieve and decrypt the evidence ticket. */
    memset(&mcred, 0, sizeof(mcred));
    mcred.client = client_name;
    mcred.server = int_name;
    check(krb5_cc_retrieve_cred(context, evcc, 0, &mcred, &ev_cred));
    check(krb5_decode_ticket(&ev_cred.ticket, &ev_ticket));
    check(krb5_server_decrypt_ticket_keytab(context, defkt, ev_ticket));

    /* Make an S4U2Proxy request for the target service. */
    mcred.client = client_name;
    mcred.server = target_name;
    mcred.authdata = req_authdata;
    check(krb5_get_credentials_for_proxy(context, KRB5_GC_NO_STORE |
                                         KRB5_GC_CANONICALIZE, defcc,
                                         &mcred, ev_ticket, &new_cred));

    assert(data_eq(new_cred->second_ticket, ev_cred.ticket));
    assert(new_cred->second_ticket.length != 0);

    /* Store the new cred in the default ccache. */
    check(krb5_cc_store_cred(context, defcc, new_cred));

    assert(req_authdata == NULL || new_cred->authdata != NULL);

    krb5_cc_close(context, defcc);
    krb5_cc_close(context, evcc);
    krb5_kt_close(context, defkt);
    krb5_free_principal(context, client_name);
    krb5_free_principal(context, int_name);
    krb5_free_principal(context, target_name);
    krb5_free_cred_contents(context, &ev_cred);
    krb5_free_ticket(context, ev_ticket);
    krb5_free_creds(context, new_cred);
    krb5_free_authdata(context, req_authdata);
    krb5_free_context(context);
    return 0;
}
