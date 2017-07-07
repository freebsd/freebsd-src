/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/test/kdctest.c - Test kdcpreauth module */
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
 * This module is used to test preauth interface features.  Currently, the
 * kdcpreauth module does the following:
 *
 * - When generating initial method-data, it retrieves the "teststring"
 *   attribute from the client principal and sends it to the client, encrypted
 *   in the reply key.  (The plain text "no key" is sent if there is no reply
 *   key; the encrypted message "no attr" is sent if there is no string
 *   attribute.)  It also sets a cookie containing "method-data".
 *
 * - It retrieves the "2rt" attribute from the client principal.  If set, the
 *   verify method sends the client a KDC_ERR_MORE_PREAUTH_DATA_REQUIRED error
 *   with the contents of the 2rt attribute as pa-data, and sets a cookie
 *   containing "more".
 *
 * - It receives a space-separated list from the clpreauth module and asserts
 *   each string as an authentication indicator.  It always succeeds in
 *   pre-authenticating the request.
 */

#include "k5-int.h"
#include <krb5/kdcpreauth_plugin.h>

#define TEST_PA_TYPE -123

static krb5_preauthtype pa_types[] = { TEST_PA_TYPE, 0 };

static void
test_edata(krb5_context context, krb5_kdc_req *req,
           krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
           krb5_kdcpreauth_moddata moddata, krb5_preauthtype pa_type,
           krb5_kdcpreauth_edata_respond_fn respond, void *arg)
{
    krb5_error_code ret;
    const krb5_keyblock *k = cb->client_keyblock(context, rock);
    krb5_pa_data *pa;
    size_t enclen;
    krb5_enc_data enc;
    krb5_data d;
    char *attr;

    ret = cb->get_string(context, rock, "teststring", &attr);
    assert(!ret);
    pa = k5alloc(sizeof(*pa), &ret);
    assert(!ret);
    if (pa == NULL)
        abort();
    pa->pa_type = TEST_PA_TYPE;
    if (k != NULL) {
        d = string2data((attr != NULL) ? attr : "no attr");
        ret = krb5_c_encrypt_length(context, k->enctype, d.length, &enclen);
        assert(!ret);
        ret = alloc_data(&enc.ciphertext, enclen);
        assert(!ret);
        ret = krb5_c_encrypt(context, k, 1024, NULL, &d, &enc);
        assert(!ret);
        pa->contents = (uint8_t *)enc.ciphertext.data;
        pa->length = enc.ciphertext.length;
    } else {
        pa->contents = (uint8_t *)strdup("no key");
        assert(pa->contents != NULL);
        pa->length = 6;
    }

    /* Exercise setting a cookie information from the edata method. */
    d = string2data("method-data");
    ret = cb->set_cookie(context, rock, TEST_PA_TYPE, &d);
    assert(!ret);

    cb->free_string(context, rock, attr);
    (*respond)(arg, 0, pa);
}

static void
test_verify(krb5_context context, krb5_data *req_pkt, krb5_kdc_req *request,
            krb5_enc_tkt_part *enc_tkt_reply, krb5_pa_data *data,
            krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
            krb5_kdcpreauth_moddata moddata,
            krb5_kdcpreauth_verify_respond_fn respond, void *arg)
{
    krb5_error_code ret;
    krb5_boolean second_round_trip = FALSE;
    krb5_pa_data **list;
    krb5_data cookie_data, d;
    char *str, *ind, *attr, *toksave = NULL;

    ret = cb->get_string(context, rock, "2rt", &attr);
    assert(!ret);

    /* Check the incoming cookie value. */
    if (!cb->get_cookie(context, rock, TEST_PA_TYPE, &cookie_data)) {
        /* Make sure we are seeing optimistic preauth and not a lost cookie. */
        d = make_data(data->contents, data->length);
        assert(data_eq_string(d, "optimistic"));
    } else if (data_eq_string(cookie_data, "more")) {
        second_round_trip = TRUE;
    } else {
        assert(data_eq_string(cookie_data, "method-data"));
    }

    if (attr == NULL || second_round_trip) {
        /* Parse and assert the indicators. */
        str = k5memdup0(data->contents, data->length, &ret);
        if (ret)
            abort();
        ind = strtok_r(str, " ", &toksave);
        while (ind != NULL) {
            cb->add_auth_indicator(context, rock, ind);
            ind = strtok_r(NULL, " ", &toksave);
        }
        free(str);
        enc_tkt_reply->flags |= TKT_FLG_PRE_AUTH;
        cb->free_string(context, rock, attr);
        (*respond)(arg, 0, NULL, NULL, NULL);
    } else {
        d = string2data("more");
        ret = cb->set_cookie(context, rock, TEST_PA_TYPE, &d);
        list = k5calloc(2, sizeof(*list), &ret);
        assert(!ret);
        list[0] = k5alloc(sizeof(*list[0]), &ret);
        assert(!ret);
        list[0]->pa_type = TEST_PA_TYPE;
        list[0]->contents = (uint8_t *)attr;
        list[0]->length = strlen(attr);
        (*respond)(arg, KRB5KDC_ERR_MORE_PREAUTH_DATA_REQUIRED, NULL, list,
                   NULL);
    }
}

static krb5_error_code
test_return(krb5_context context, krb5_pa_data *padata, krb5_data *req_pkt,
            krb5_kdc_req *request, krb5_kdc_rep *reply,
            krb5_keyblock *encrypting_key, krb5_pa_data **send_pa_out,
            krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
            krb5_kdcpreauth_moddata moddata, krb5_kdcpreauth_modreq modreq)
{
    const krb5_keyblock *k = cb->client_keyblock(context, rock);

    assert(k == encrypting_key || k == NULL);
    return 0;
}

krb5_error_code
kdcpreauth_test_initvt(krb5_context context, int maj_ver,
                             int min_ver, krb5_plugin_vtable vtable);

krb5_error_code
kdcpreauth_test_initvt(krb5_context context, int maj_ver,
                             int min_ver, krb5_plugin_vtable vtable)
{
    krb5_kdcpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_kdcpreauth_vtable)vtable;
    vt->name = "test";
    vt->pa_type_list = pa_types;
    vt->edata = test_edata;
    vt->verify = test_verify;
    vt->return_padata = test_return;
    return 0;
}
