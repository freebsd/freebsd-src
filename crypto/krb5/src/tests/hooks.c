/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/hooks.c - test harness for KDC send and recv hooks */
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

#include "k5-int.h"

static krb5_context ctx;

static void
check_code(krb5_error_code code, const char *file, int line)
{
    const char *errmsg;

    if (code) {
        errmsg = krb5_get_error_message(ctx, code);
        fprintf(stderr, "%s:%d -- %s (code=%d)\n", file, line, errmsg,
                (int)code);
        krb5_free_error_message(ctx, errmsg);
        exit(1);
    }
}

#define check(code) check_code((code), __FILE__, __LINE__)

/* Verify that the canonicalize bit is set in an AS-REQ and remove it. */
static krb5_error_code
test_send_as_req(krb5_context context, void *data, const krb5_data *realm,
                 const krb5_data *message, krb5_data **new_message_out,
                 krb5_data **reply_out)
{
    krb5_kdc_req *as_req;
    int cmp;

    assert(krb5_is_as_req(message));
    check(decode_krb5_as_req(message, &as_req));

    assert(as_req->msg_type == KRB5_AS_REQ);
    assert(as_req->kdc_options & KDC_OPT_CANONICALIZE);
    assert(as_req->client->realm.length == realm->length);
    cmp = memcmp(as_req->client->realm.data, realm->data, realm->length);
    assert(cmp == 0);

    /* Remove the canonicalize flag and create a new message. */
    as_req->kdc_options &= ~KDC_OPT_CANONICALIZE;
    check(encode_krb5_as_req(as_req, new_message_out));

    krb5_free_kdc_req(context, as_req);
    return 0;
}

/* Verify that reply is an AS-REP with kvno 1 and a valid enctype. */
static krb5_error_code
test_recv_as_rep(krb5_context context, void *data, krb5_error_code code,
                 const krb5_data *realm, const krb5_data *message,
                 const krb5_data *reply, krb5_data **new_reply)
{
    krb5_kdc_rep *as_rep;

    assert(code == 0);
    assert(krb5_is_as_rep(reply));
    check(decode_krb5_as_rep(reply, &as_rep));

    assert(as_rep->msg_type == KRB5_AS_REP);
    assert(as_rep->ticket->enc_part.kvno == 1);
    assert(krb5_c_valid_enctype(as_rep->ticket->enc_part.enctype));

    krb5_free_kdc_rep(context, as_rep);
    return 0;
}

/* Create a fake error reply. */
static krb5_error_code
test_send_error(krb5_context context, void *data, const krb5_data *realm,
                const krb5_data *message, krb5_data **new_message_out,
                krb5_data **reply_out)
{
    krb5_error_code ret;
    krb5_error err;
    krb5_principal client, server;
    char *realm_str, *princ_str;
    int r;

    realm_str = k5memdup0(realm->data, realm->length, &ret);
    check(ret);

    r = asprintf(&princ_str, "invalid@%s", realm_str);
    assert(r > 0);
    check(krb5_parse_name(ctx, princ_str, &client));
    free(princ_str);

    r = asprintf(&princ_str, "krbtgt@%s", realm_str);
    assert(r > 0);
    check(krb5_parse_name(ctx, princ_str, &server));
    free(princ_str);
    free(realm_str);

    err.magic = KV5M_ERROR;
    err.ctime = 1971196337;
    err.cusec = 0;
    err.susec = 97008;
    err.stime = 1458219390;
    err.error = 6;
    err.client = client;
    err.server = server;
    err.text = string2data("CLIENT_NOT_FOUND");
    err.e_data = empty_data();
    check(encode_krb5_error(&err, reply_out));

    krb5_free_principal(ctx, client);
    krb5_free_principal(ctx, server);
    return 0;
}

static krb5_error_code
test_recv_error(krb5_context context, void *data, krb5_error_code code,
                     const krb5_data *realm, const krb5_data *message,
                     const krb5_data *reply, krb5_data **new_reply)
{
    /* The send hook created a reply, so this hook should not be executed. */
    abort();
}

/* Modify an AS-REP reply, change the msg_type to KRB5_TGS_REP. */
static krb5_error_code
test_recv_modify_reply(krb5_context context, void *data, krb5_error_code code,
                       const krb5_data *realm, const krb5_data *message,
                       const krb5_data *reply, krb5_data **new_reply)
{
    krb5_kdc_rep *as_rep;

    assert(code == 0);
    assert(krb5_is_as_rep(reply));
    check(decode_krb5_as_rep(reply, &as_rep));

    as_rep->msg_type = KRB5_TGS_REP;
    check(encode_krb5_as_rep(as_rep, new_reply));

    krb5_free_kdc_rep(context, as_rep);
    return 0;
}

/* Return an error given by the callback data argument. */
static krb5_error_code
test_send_return_value(krb5_context context, void *data,
                       const krb5_data *realm, const krb5_data *message,
                       krb5_data **new_message_out, krb5_data **reply_out)
{
    assert(data != NULL);
    return *(krb5_error_code *)data;
}

/* Return an error given by the callback argument. */
static krb5_error_code
test_recv_return_value(krb5_context context, void *data, krb5_error_code code,
                       const krb5_data *realm, const krb5_data *message,
                       const krb5_data *reply, krb5_data **new_reply)
{
    assert(data != NULL);
    return *(krb5_error_code *)data;
}

int
main(int argc, char *argv[])
{
    const char *principal, *password;
    krb5_principal client;
    krb5_get_init_creds_opt *opts;
    krb5_creds creds;
    krb5_error_code ret, test_return_code;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s princname password\n", argv[0]);
        exit(1);
    }
    principal = argv[1];
    password = argv[2];

    check(krb5_init_context(&ctx));
    check(krb5_parse_name(ctx, principal, &client));

    /* Use a send hook to modify an outgoing AS-REQ.  The library will detect
     * the modification in the reply. */
    check(krb5_get_init_creds_opt_alloc(ctx, &opts));
    krb5_get_init_creds_opt_set_canonicalize(opts, 1);
    krb5_set_kdc_send_hook(ctx, test_send_as_req, NULL);
    krb5_set_kdc_recv_hook(ctx, test_recv_as_rep, NULL);
    ret = krb5_get_init_creds_password(ctx, &creds, client, password, NULL,
                                       NULL, 0, NULL, opts);
    assert(ret == KRB5_KDCREP_MODIFIED);
    krb5_get_init_creds_opt_free(ctx, opts);

    /* Use a send hook to synthesize a KRB-ERROR reply. */
    krb5_set_kdc_send_hook(ctx, test_send_error, NULL);
    krb5_set_kdc_recv_hook(ctx, test_recv_error, NULL);
    ret = krb5_get_init_creds_password(ctx, &creds, client, password, NULL,
                                       NULL, 0, NULL, NULL);
    assert(ret == KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN);

    /* Use a recv hook to modify a KDC reply. */
    krb5_set_kdc_send_hook(ctx, NULL, NULL);
    krb5_set_kdc_recv_hook(ctx, test_recv_modify_reply, NULL);
    ret = krb5_get_init_creds_password(ctx, &creds, client, password, NULL,
                                       NULL, 0, NULL, NULL);
    assert(ret == KRB5KRB_AP_ERR_MSG_TYPE);

    /* Verify that the user data pointer works in the send hook. */
    test_return_code = KRB5KDC_ERR_PREAUTH_FAILED;
    krb5_set_kdc_send_hook(ctx, test_send_return_value, &test_return_code);
    krb5_set_kdc_recv_hook(ctx, NULL, NULL);
    ret = krb5_get_init_creds_password(ctx, &creds, client, password, NULL,
                                        NULL, 0, NULL, NULL);
    assert(ret == KRB5KDC_ERR_PREAUTH_FAILED);

    /* Verify that the user data pointer works in the recv hook. */
    test_return_code = KRB5KDC_ERR_NULL_KEY;
    krb5_set_kdc_send_hook(ctx, NULL, NULL);
    krb5_set_kdc_recv_hook(ctx, test_recv_return_value, &test_return_code);
    ret = krb5_get_init_creds_password(ctx, &creds, client, password, NULL,
                                       NULL, 0, NULL, NULL);
    assert(ret == KRB5KDC_ERR_NULL_KEY);

    krb5_free_principal(ctx, client);
    krb5_free_context(ctx);
    return 0;
}
