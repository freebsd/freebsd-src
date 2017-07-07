/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/test/cltest.c - Test clpreauth module */
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
 * This module is used to test preauth interface features.  At this time, the
 * clpreauth module does two things:
 *
 * - It decrypts a message from the initial KDC pa-data using the reply key and
 *   prints it to stdout.  (The unencrypted message "no key" can also be
 *   displayed.)
 *
 * - If a second round trip is requested, it prints the pa-data contents
 *   accompanying the second round trip request.
 *
 * - It pulls an "indicators" attribute from the gic preauth options and sends
 *   it to the server, instructing the kdcpreauth module to assert one or more
 *   space-separated authentication indicators.  (This string is sent on both
 *   round trips if a second round trip is requested.)
 */

#include "k5-int.h"
#include <krb5/clpreauth_plugin.h>

#define TEST_PA_TYPE -123

static krb5_preauthtype pa_types[] = { TEST_PA_TYPE, 0 };

struct client_state {
    char *indicators;
};

struct client_request_state {
    krb5_boolean second_round_trip;
};

static krb5_error_code
test_init(krb5_context context, krb5_clpreauth_moddata *moddata_out)
{
    struct client_state *st;

    st = malloc(sizeof(*st));
    assert(st != NULL);
    st->indicators = NULL;
    *moddata_out = (krb5_clpreauth_moddata)st;
    return 0;
}

static void
test_fini(krb5_context context, krb5_clpreauth_moddata moddata)
{
    struct client_state *st = (struct client_state *)moddata;

    free(st->indicators);
    free(st);
}

static void
test_request_init(krb5_context context, krb5_clpreauth_moddata moddata,
                  krb5_clpreauth_modreq *modreq_out)
{
    struct client_request_state *reqst;

    reqst = malloc(sizeof(*reqst));
    assert(reqst != NULL);
    reqst->second_round_trip = FALSE;
    *modreq_out = (krb5_clpreauth_modreq)reqst;
}

static void
test_request_fini(krb5_context context, krb5_clpreauth_moddata moddata,
                  krb5_clpreauth_modreq modreq)
{
    free(modreq);
}

static krb5_error_code
test_process(krb5_context context, krb5_clpreauth_moddata moddata,
             krb5_clpreauth_modreq modreq, krb5_get_init_creds_opt *opt,
             krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
             krb5_kdc_req *request, krb5_data *encoded_request_body,
             krb5_data *encoded_previous_request, krb5_pa_data *pa_data,
             krb5_prompter_fct prompter, void *prompter_data,
             krb5_pa_data ***out_pa_data)
{
    struct client_state *st = (struct client_state *)moddata;
    struct client_request_state *reqst = (struct client_request_state *)modreq;
    krb5_error_code ret;
    krb5_pa_data **list, *pa;
    krb5_keyblock *k;
    krb5_enc_data enc;
    krb5_data plain;
    const char *indstr;

    if (pa_data->length == 0) {
        /* This is an optimistic preauth test.  Send a recognizable padata
         * value so the KDC knows not to expect a cookie. */
        list = k5calloc(2, sizeof(*list), &ret);
        assert(!ret);
        pa = k5alloc(sizeof(*pa), &ret);
        assert(!ret);
        pa->pa_type = TEST_PA_TYPE;
        pa->contents = (uint8_t *)strdup("optimistic");
        assert(pa->contents != NULL);
        pa->length = 10;
        list[0] = pa;
        list[1] = NULL;
        *out_pa_data = list;
        return 0;
    } else if (reqst->second_round_trip) {
        printf("2rt: %.*s\n", pa_data->length, pa_data->contents);
    } else if (pa_data->length == 6 &&
               memcmp(pa_data->contents, "no key", 6) == 0) {
        printf("no key\n");
    } else {
        /* This fails during s4u_identify_user(), so don't assert. */
        ret = cb->get_as_key(context, rock, &k);
        if (ret)
            return ret;
        ret = alloc_data(&plain, pa_data->length);
        assert(!ret);
        enc.enctype = k->enctype;
        enc.ciphertext = make_data(pa_data->contents, pa_data->length);
        ret = krb5_c_decrypt(context, k, 1024, NULL, &enc, &plain);
        assert(!ret);
        printf("%.*s\n", plain.length, plain.data);
        free(plain.data);
    }
    reqst->second_round_trip = TRUE;

    indstr = (st->indicators != NULL) ? st->indicators : "";
    list = k5calloc(2, sizeof(*list), &ret);
    assert(!ret);
    pa = k5alloc(sizeof(*pa), &ret);
    assert(!ret);
    pa->pa_type = TEST_PA_TYPE;
    pa->contents = (uint8_t *)strdup(indstr);
    assert(pa->contents != NULL);
    pa->length = strlen(indstr);
    list[0] = pa;
    list[1] = NULL;
    *out_pa_data = list;
    return 0;
}

static krb5_error_code
test_gic_opt(krb5_context kcontext, krb5_clpreauth_moddata moddata,
             krb5_get_init_creds_opt *opt, const char *attr, const char *value)
{
    struct client_state *st = (struct client_state *)moddata;

    if (strcmp(attr, "indicators") == 0) {
        free(st->indicators);
        st->indicators = strdup(value);
        assert(st->indicators != NULL);
    }
    return 0;
}

krb5_error_code
clpreauth_test_initvt(krb5_context context, int maj_ver,
                            int min_ver, krb5_plugin_vtable vtable);

krb5_error_code
clpreauth_test_initvt(krb5_context context, int maj_ver,
                            int min_ver, krb5_plugin_vtable vtable)
{
    krb5_clpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_clpreauth_vtable)vtable;
    vt->name = "test";
    vt->pa_type_list = pa_types;
    vt->init = test_init;
    vt->fini = test_fini;
    vt->request_init = test_request_init;
    vt->request_fini = test_request_fini;
    vt->process = test_process;
    vt->gic_opts = test_gic_opt;
    return 0;
}
