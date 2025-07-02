/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/replay.c - test replay cache using libkrb5 functions */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context ctx;
    krb5_auth_context c_authcon, s_authcon, s_authcon2;
    krb5_rcache rc;
    krb5_ccache cc;
    krb5_principal client, server;
    krb5_creds mcred, *cred, **tmpcreds;
    krb5_data der_apreq, der_krbsafe, der_krbpriv, *der_krbcred, tmpdata;
    krb5_address addr;
    struct in_addr inaddr;
    const char *server_name;

    assert(argc == 2);
    server_name = argv[1];

    /* Create client and server auth contexts.  (They will use a replay cache
     * by default.) */
    ret = krb5_init_context(&ctx);
    assert(ret == 0);
    ret = krb5_auth_con_init(ctx, &c_authcon);
    assert(ret == 0);
    ret = krb5_auth_con_init(ctx, &s_authcon);
    assert(ret == 0);

    /* Set dummy addresses for the auth contexts. */
    memset(&inaddr, 0, sizeof(inaddr));
    addr.addrtype = ADDRTYPE_INET;
    addr.length = sizeof(inaddr);
    addr.contents = (uint8_t *)&inaddr;
    ret = krb5_auth_con_setaddrs(ctx, c_authcon, &addr, &addr);
    assert(ret == 0);
    ret = krb5_auth_con_setaddrs(ctx, s_authcon, &addr, &addr);
    assert(ret == 0);

    /* Set up replay caches for the auth contexts. */
    tmpdata = string2data("testclient");
    ret = krb5_get_server_rcache(ctx, &tmpdata, &rc);
    assert(ret == 0);
    ret = krb5_auth_con_setrcache(ctx, c_authcon, rc);
    assert(ret == 0);
    tmpdata = string2data("testserver");
    ret = krb5_get_server_rcache(ctx, &tmpdata, &rc);
    assert(ret == 0);
    ret = krb5_auth_con_setrcache(ctx, s_authcon, rc);
    assert(ret == 0);

    /* Construct the client and server principal names. */
    ret = krb5_cc_default(ctx, &cc);
    assert(ret == 0);
    ret = krb5_cc_get_principal(ctx, cc, &client);
    assert(ret == 0);
    ret = krb5_parse_name(ctx, server_name, &server);
    assert(ret == 0);

    /* Get credentials for the client. */
    memset(&mcred, 0, sizeof(mcred));
    mcred.client = client;
    mcred.server = server;
    ret = krb5_get_credentials(ctx, 0, cc, &mcred, &cred);
    assert(ret == 0);

    /* Send an AP-REP to establish the sessions. */
    ret = krb5_mk_req_extended(ctx, &c_authcon, 0, NULL, cred, &der_apreq);
    assert(ret == 0);
    ret = krb5_rd_req(ctx, &s_authcon, &der_apreq, NULL, NULL, NULL, NULL);
    assert(ret == 0);

    /* Set up another server auth context with the same rcache name and replay
     * the AP-REQ. */
    ret = krb5_auth_con_init(ctx, &s_authcon2);
    assert(ret == 0);
    tmpdata = string2data("testserver");
    ret = krb5_get_server_rcache(ctx, &tmpdata, &rc);
    assert(ret == 0);
    ret = krb5_auth_con_setrcache(ctx, s_authcon2, rc);
    assert(ret == 0);
    ret = krb5_rd_req(ctx, &s_authcon2, &der_apreq, NULL, NULL, NULL, NULL);
    assert(ret == KRB5KRB_AP_ERR_REPEAT);
    krb5_auth_con_free(ctx, s_authcon2);

    /* Make a KRB-SAFE message with the client auth context. */
    tmpdata = string2data("safemsg");
    ret = krb5_mk_safe(ctx, c_authcon, &tmpdata, &der_krbsafe, NULL);
    assert(ret == 0);
    /* Play it back to the client to detect a reflection. */
    ret = krb5_rd_safe(ctx, c_authcon, &der_krbsafe, &tmpdata, NULL);
    assert(ret == KRB5KRB_AP_ERR_REPEAT);
    /* Send it to the server auth context twice, to detect a replay. */
    ret = krb5_rd_safe(ctx, s_authcon, &der_krbsafe, &tmpdata, NULL);
    assert(ret == 0);
    krb5_free_data_contents(ctx, &tmpdata);
    ret = krb5_rd_safe(ctx, s_authcon, &der_krbsafe, &tmpdata, NULL);
    assert(ret == KRB5KRB_AP_ERR_REPEAT);

    /* Make a KRB-PRIV message with the client auth context. */
    tmpdata = string2data("safemsg");
    ret = krb5_mk_priv(ctx, c_authcon, &tmpdata, &der_krbpriv, NULL);
    assert(ret == 0);
    /* Play it back to the client to detect a reflection. */
    ret = krb5_rd_priv(ctx, c_authcon, &der_krbpriv, &tmpdata, NULL);
    assert(ret == KRB5KRB_AP_ERR_REPEAT);
    /* Send it to the server auth context twice, to detect a replay. */
    ret = krb5_rd_priv(ctx, s_authcon, &der_krbpriv, &tmpdata, NULL);
    assert(ret == 0);
    krb5_free_data_contents(ctx, &tmpdata);
    ret = krb5_rd_priv(ctx, s_authcon, &der_krbpriv, &tmpdata, NULL);
    assert(ret == KRB5KRB_AP_ERR_REPEAT);

    /* Make a KRB-CRED message with the client auth context. */
    tmpdata = string2data("safemsg");
    ret = krb5_mk_1cred(ctx, c_authcon, cred, &der_krbcred, NULL);
    assert(ret == 0);
    /* Play it back to the client to detect a reflection. */
    ret = krb5_rd_cred(ctx, c_authcon, der_krbcred, &tmpcreds, NULL);
    assert(ret == KRB5KRB_AP_ERR_REPEAT);
    /* Send it to the server auth context twice, to detect a replay. */
    ret = krb5_rd_cred(ctx, s_authcon, der_krbcred, &tmpcreds, NULL);
    assert(ret == 0);
    krb5_free_tgt_creds(ctx, tmpcreds);
    ret = krb5_rd_cred(ctx, s_authcon, der_krbcred, &tmpcreds, NULL);
    assert(ret == KRB5KRB_AP_ERR_REPEAT);

    krb5_free_data_contents(ctx, &der_apreq);
    krb5_free_data_contents(ctx, &der_krbsafe);
    krb5_free_data_contents(ctx, &der_krbpriv);
    krb5_free_data(ctx, der_krbcred);
    krb5_free_creds(ctx, cred);
    krb5_cc_close(ctx, cc);
    krb5_free_principal(ctx, client);
    krb5_free_principal(ctx, server);
    krb5_auth_con_free(ctx, c_authcon);
    krb5_auth_con_free(ctx, s_authcon);
    krb5_free_context(ctx);
    return 0;
}
