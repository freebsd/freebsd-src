/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/rdreq.c - Test harness for krb5_rd_req */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <krb5.h>

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_principal client_princ, tkt_princ, server_princ;
    krb5_ccache ccache;
    krb5_creds *cred, mcred;
    krb5_auth_context auth_con;
    krb5_data apreq;
    krb5_error_code ret, code;
    const char *tkt_name, *server_name, *emsg;

    /* Parse arguments. */
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: rdreq tktname [servername]\n");
        exit(1);
    }
    tkt_name = argv[1];
    server_name = argv[2];

    if (krb5_init_context(&context) != 0)
        abort();

    /* Parse the requested principal names. */
    if (krb5_parse_name(context, tkt_name, &tkt_princ) != 0)
        abort();
    if (server_name != NULL) {
        if (krb5_parse_name(context, server_name, &server_princ) != 0)
            abort();
        server_princ->type = KRB5_NT_SRV_HST;
    } else {
        server_princ = NULL;
    }

    /* Produce an AP-REQ message. */
    if (krb5_cc_default(context, &ccache) != 0)
        abort();
    if (krb5_cc_get_principal(context, ccache, &client_princ) != 0)
        abort();
    memset(&mcred, 0, sizeof(mcred));
    mcred.client = client_princ;
    mcred.server = tkt_princ;
    if (krb5_get_credentials(context, 0, ccache, &mcred, &cred) != 0)
        abort();
    auth_con = NULL;
    if (krb5_mk_req_extended(context, &auth_con, 0, NULL, cred, &apreq) != 0)
        abort();

    /* Consume the AP-REQ message without using a replay cache. */
    krb5_auth_con_free(context, auth_con);
    if (krb5_auth_con_init(context, &auth_con) != 0)
        abort();
    if (krb5_auth_con_setflags(context, auth_con, 0) != 0)
        abort();
    ret = krb5_rd_req(context, &auth_con, &apreq, server_princ, NULL, NULL,
                      NULL);

    /* Display the result. */
    if (ret) {
        code = ret - ERROR_TABLE_BASE_krb5;
        if (code < 0 || code > 127)
            code = 60;          /* KRB_ERR_GENERIC */
        emsg = krb5_get_error_message(context, ret);
        printf("%d %s\n", code, emsg);
        krb5_free_error_message(context, emsg);
    } else {
        printf("0 success\n");
    }

    krb5_free_data_contents(context, &apreq);
    assert(apreq.length == 0);
    krb5_auth_con_free(context, auth_con);
    krb5_free_creds(context, cred);
    krb5_cc_close(context, ccache);
    krb5_free_principal(context, client_princ);
    krb5_free_principal(context, tkt_princ);
    krb5_free_principal(context, server_princ);
    krb5_free_context(context);
    return 0;
}
