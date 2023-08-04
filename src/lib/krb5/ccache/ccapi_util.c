/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/ccapi_util.c - conversion functions for CCAPI creds */
/*
 * Copyright (C) 2022 by the Massachusetts Institute of Technology.
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

#include "cc-int.h"
#include "ccapi_util.h"

#if defined(USE_CCAPI) || defined(USE_CCAPI_MACOS)

static void
free_cc_data_list(cc_data **list)
{
    size_t i;

    for (i = 0; list != NULL && list[i] != NULL; i++) {
        free(list[i]->data);
        free(list[i]);
    }
    free(list);
}

static krb5_error_code
cc_data_list_to_addresses(krb5_context context, cc_data **list,
                          krb5_address ***addrs_out)
{
    krb5_error_code ret;
    size_t count, i;
    krb5_address **addrs = NULL;

    *addrs_out = NULL;
    if (list == NULL)
        return 0;

    for (count = 0; list[count]; count++);
    addrs = k5calloc(count + 1, sizeof(*addrs), &ret);
    if (addrs == NULL)
        return ret;

    for (i = 0; i < count; i++) {
        addrs[i] = k5alloc(sizeof(*addrs[i]), &ret);
        if (addrs[i] == NULL)
            goto cleanup;

        addrs[i]->contents = k5memdup(list[i]->data, list[i]->length, &ret);
        if (addrs[i]->contents == NULL)
            goto cleanup;
        addrs[i]->length = list[i]->length;
        addrs[i]->addrtype = list[i]->type;
        addrs[i]->magic = KV5M_ADDRESS;
    }

    *addrs_out = addrs;
    addrs = NULL;

cleanup:
    krb5_free_addresses(context, addrs);
    return ret;
}

static krb5_error_code
cc_data_list_to_authdata(krb5_context context, cc_data **list,
                         krb5_authdata ***authdata_out)
{
    krb5_error_code ret;
    size_t count, i;
    krb5_authdata **authdata = NULL;

    *authdata_out = NULL;
    if (list == NULL)
        return 0;

    for (count = 0; list[count]; count++);
    authdata = k5calloc(count + 1, sizeof(*authdata), &ret);
    if (authdata == NULL)
        return ret;

    for (i = 0; i < count; i++) {
        authdata[i] = k5alloc(sizeof(*authdata[i]), &ret);
        if (authdata[i] == NULL)
            goto cleanup;

        authdata[i]->contents = k5memdup(list[i]->data, list[i]->length, &ret);
        if (authdata[i]->contents == NULL)
            goto cleanup;
        authdata[i]->length = list[i]->length;
        authdata[i]->ad_type = list[i]->type;
        authdata[i]->magic = KV5M_AUTHDATA;
    }

    *authdata_out = authdata;
    authdata = NULL;

cleanup:
    krb5_free_authdata(context, authdata);
    return ret;
}

static krb5_error_code
addresses_to_cc_data_list(krb5_context context, krb5_address **addrs,
                          cc_data ***list_out)
{
    krb5_error_code ret;
    size_t count, i;
    cc_data **list = NULL;

    *list_out = NULL;
    if (addrs == NULL)
        return 0;

    for (count = 0; addrs[count]; count++);
    list = k5calloc(count + 1, sizeof(*list), &ret);
    if (list == NULL)
        return ret;

    for (i = 0; i < count; i++) {
        list[i] = k5alloc(sizeof(*list[i]), &ret);
        if (list[i] == NULL)
            goto cleanup;

        list[i]->data = k5memdup(addrs[i]->contents, addrs[i]->length, &ret);
        if (list[i]->data == NULL)
            goto cleanup;
        list[i]->length = addrs[i]->length;
        list[i]->type = addrs[i]->addrtype;
    }

    *list_out = list;
    list = NULL;

cleanup:
    free_cc_data_list(list);
    return ret;
}

static krb5_error_code
authdata_to_cc_data_list(krb5_context context, krb5_authdata **authdata,
                         cc_data ***list_out)
{
    krb5_error_code ret;
    size_t count, i;
    cc_data **list = NULL;

    *list_out = NULL;
    if (authdata == NULL)
        return 0;

    for (count = 0; authdata[count]; count++);
    list = k5calloc(count + 1, sizeof(*list), &ret);
    if (list == NULL)
        return ret;

    for (i = 0; i < count; i++) {
        list[i] = k5alloc(sizeof(*list[i]), &ret);
        if (list[i] == NULL)
            goto cleanup;

        list[i]->data = k5memdup(authdata[i]->contents, authdata[i]->length,
                                 &ret);
        if (list[i]->data == NULL)
            goto cleanup;
        list[i]->length = authdata[i]->length;
        list[i]->type = authdata[i]->ad_type;
    }

    *list_out = list;
    list = NULL;

cleanup:
    free_cc_data_list(list);
    return ret;
}

krb5_error_code
k5_ccapi_to_krb5_creds(krb5_context context,
                       const cc_credentials_union *ccapi_cred,
                       krb5_creds *cred_out)
{
    krb5_error_code ret;
    cc_credentials_v5_t *cv5 = NULL;
    krb5_principal client = NULL;
    krb5_principal server = NULL;
    char *ticket_data = NULL;
    char *second_ticket_data = NULL;
    uint8_t *keyblock_contents = NULL;
    krb5_address **addresses = NULL;
    krb5_authdata **authdata = NULL;

    if (ccapi_cred->version != cc_credentials_v5)
        return KRB5_CC_NOT_KTYPE;

    cv5 = ccapi_cred->credentials.credentials_v5;

    ret = krb5_parse_name(context, cv5->client, &client);
    if (ret)
        goto cleanup;
    ret = krb5_parse_name(context, cv5->server, &server);
    if (ret)
        goto cleanup;

    if (cv5->keyblock.length > 0) {
        keyblock_contents = k5memdup(cv5->keyblock.data, cv5->keyblock.length,
                                     &ret);
        if (keyblock_contents == NULL)
            goto cleanup;
    }

    if (cv5->ticket.length > 0) {
        ticket_data = k5memdup(cv5->ticket.data, cv5->ticket.length, &ret);
        if (ticket_data == NULL)
            goto cleanup;
    }

    if (cv5->second_ticket.length > 0) {
        second_ticket_data = k5memdup(cv5->second_ticket.data,
                                      cv5->second_ticket.length, &ret);
        if (second_ticket_data == NULL)
            goto cleanup;
    }

    ret = cc_data_list_to_addresses(context, cv5->addresses, &addresses);
    if (ret)
        goto cleanup;

    ret = cc_data_list_to_authdata(context, cv5->authdata, &authdata);
    if (ret)
        goto cleanup;

    cred_out->client = client;
    cred_out->server = server;
    client = server = NULL;

    cred_out->keyblock.magic = KV5M_KEYBLOCK;
    cred_out->keyblock.enctype = cv5->keyblock.type;
    cred_out->keyblock.length = cv5->keyblock.length;
    cred_out->keyblock.contents = keyblock_contents;
    keyblock_contents = NULL;

    cred_out->times.authtime = cv5->authtime;
    cred_out->times.starttime = cv5->starttime;
    cred_out->times.endtime = cv5->endtime;
    cred_out->times.renew_till = cv5->renew_till;
    cred_out->is_skey = cv5->is_skey;
    cred_out->ticket_flags = cv5->ticket_flags;

    cred_out->ticket = make_data(ticket_data, cv5->ticket.length);
    cred_out->second_ticket = make_data(second_ticket_data,
                                        cv5->second_ticket.length);
    ticket_data = second_ticket_data = NULL;

    cred_out->addresses = addresses;
    addresses = NULL;

    cred_out->authdata = authdata;
    authdata = NULL;

    cred_out->magic = KV5M_CREDS;

cleanup:
    krb5_free_principal(context, client);
    krb5_free_principal(context, server);
    krb5_free_addresses(context, addresses);
    krb5_free_authdata(context, authdata);
    free(keyblock_contents);
    free(ticket_data);
    free(second_ticket_data);
    return ret;
}

krb5_error_code
k5_krb5_to_ccapi_creds(krb5_context context, krb5_creds *cred,
                       cc_credentials_union **ccapi_cred_out)
{
    krb5_error_code ret;
    cc_credentials_union *cred_union = NULL;
    cc_credentials_v5_t *cv5 = NULL;
    char *client = NULL, *server = NULL;
    uint8_t *ticket_data = NULL, *second_ticket_data = NULL;
    uint8_t *keyblock_data = NULL;
    cc_data **addr_list = NULL, **authdata_list = NULL;

    cred_union = k5alloc(sizeof(*cred_union), &ret);
    if (cred_union == NULL)
        goto cleanup;

    cv5 = k5alloc(sizeof(*cv5), &ret);
    if (cv5 == NULL)
        goto cleanup;

    ret = krb5_unparse_name(context, cred->client, &client);
    if (ret)
        goto cleanup;
    ret = krb5_unparse_name(context, cred->server, &server);
    if (ret)
        goto cleanup;

    if (cred->keyblock.length > 0) {
        keyblock_data = k5memdup(cred->keyblock.contents,
                                 cred->keyblock.length, &ret);
        if (keyblock_data == NULL)
            goto cleanup;
    }

    if (cred->ticket.length > 0) {
        ticket_data = k5memdup0(cred->ticket.data, cred->ticket.length, &ret);
        if (ticket_data == NULL)
            goto cleanup;
    }

    if (cred->second_ticket.length > 0) {
        second_ticket_data = k5memdup0(cred->second_ticket.data,
                                       cred->second_ticket.length, &ret);
        if (second_ticket_data == NULL)
            goto cleanup;
    }

    ret = addresses_to_cc_data_list(context, cred->addresses, &addr_list);
    if (ret)
        goto cleanup;

    ret = authdata_to_cc_data_list(context, cred->authdata, &authdata_list);
    if (ret)
        goto cleanup;

    cv5->client = client;
    cv5->server = server;
    client = server = NULL;

    cv5->keyblock.type = cred->keyblock.enctype;
    cv5->keyblock.length = cred->keyblock.length;
    cv5->keyblock.data = keyblock_data;
    keyblock_data = NULL;

    cv5->authtime = cred->times.authtime;
    cv5->starttime = cred->times.starttime;
    cv5->endtime = cred->times.endtime;
    cv5->renew_till = cred->times.renew_till;
    cv5->is_skey = cred->is_skey;
    cv5->ticket_flags = cred->ticket_flags;

    cv5->ticket.length = cred->ticket.length;
    cv5->ticket.data = ticket_data;
    cv5->second_ticket.length = cred->second_ticket.length;
    cv5->second_ticket.data = second_ticket_data;
    ticket_data = second_ticket_data = NULL;

    cv5->addresses = addr_list;
    addr_list = NULL;

    cv5->authdata = authdata_list;
    authdata_list = NULL;

    cred_union->version = cc_credentials_v5;
    cred_union->credentials.credentials_v5 = cv5;
    cv5 = NULL;

    *ccapi_cred_out = cred_union;
    cred_union = NULL;

cleanup:
    free_cc_data_list(addr_list);
    free_cc_data_list(authdata_list);
    free(keyblock_data);
    free(ticket_data);
    free(second_ticket_data);
    krb5_free_unparsed_name(context, client);
    krb5_free_unparsed_name(context, server);
    free(cv5);
    free(cred_union);
    return ret;
}

void
k5_release_ccapi_cred(cc_credentials_union *ccapi_cred)
{
    cc_credentials_v5_t *cv5;

    if (ccapi_cred == NULL)
        return;
    if (ccapi_cred->version != cc_credentials_v5)
        return;
    if (ccapi_cred->credentials.credentials_v5 == NULL)
        return;

    cv5 = ccapi_cred->credentials.credentials_v5;

    free(cv5->client);
    free(cv5->server);
    free(cv5->keyblock.data);
    free(cv5->ticket.data);
    free(cv5->second_ticket.data);
    free_cc_data_list(cv5->addresses);
    free_cc_data_list(cv5->authdata);
    free(cv5);
    free(ccapi_cred);
}

#endif /* defined(USE_CCAPI) */
