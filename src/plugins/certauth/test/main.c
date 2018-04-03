/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/certauth/main.c - certauth plugin test modules. */
/*
 * Copyright (C) 2017 by Red Hat, Inc.
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

#include <k5-int.h>
#include "krb5/certauth_plugin.h"

struct krb5_certauth_moddata_st {
    int initialized;
};

/* Test module 1 returns OK with an indicator. */
static krb5_error_code
test1_authorize(krb5_context context, krb5_certauth_moddata moddata,
                const uint8_t *cert, size_t cert_len,
                krb5_const_principal princ, const void *opts,
                const struct _krb5_db_entry_new *db_entry,
                char ***authinds_out)
{
    char **ais = NULL;

    ais = calloc(2, sizeof(*ais));
    assert(ais != NULL);
    ais[0] = strdup("test1");
    assert(ais[0] != NULL);
    *authinds_out = ais;
    return KRB5_PLUGIN_NO_HANDLE;
}

static void
test_free_ind(krb5_context context, krb5_certauth_moddata moddata,
              char **authinds)
{
    size_t i;

    if (authinds == NULL)
        return;
    for (i = 0; authinds[i] != NULL; i++)
        free(authinds[i]);
    free(authinds);
}

/* A basic moddata test. */
static krb5_error_code
test2_init(krb5_context context, krb5_certauth_moddata *moddata_out)
{
    krb5_certauth_moddata mod;

    mod = calloc(1, sizeof(*mod));
    assert(mod != NULL);
    mod->initialized = 1;
    *moddata_out = mod;
    return 0;
}

static void
test2_fini(krb5_context context, krb5_certauth_moddata moddata)
{
    free(moddata);
}

/* Return true if cert appears to contain the CN name, based on a search of the
 * DER encoding. */
static krb5_boolean
has_cn(krb5_context context, const uint8_t *cert, size_t cert_len,
       const char *name)
{
    krb5_boolean match = FALSE;
    uint8_t name_len, cntag[5] = "\x06\x03\x55\x04\x03";
    const uint8_t *c;
    struct k5buf buf;
    size_t c_left;

    /* Construct a DER search string of the CN AttributeType encoding followed
     * by a UTF8String encoding containing name as the AttributeValue. */
    k5_buf_init_dynamic(&buf);
    k5_buf_add_len(&buf, cntag, sizeof(cntag));
    k5_buf_add(&buf, "\x0C");
    assert(strlen(name) < 128);
    name_len = strlen(name);
    k5_buf_add_len(&buf, &name_len, 1);
    k5_buf_add_len(&buf, name, name_len);
    assert(k5_buf_status(&buf) == 0);

    /* Check for the CN needle in the certificate haystack. */
    c_left = cert_len;
    c = memchr(cert, *cntag, c_left);
    while (c != NULL) {
        c_left = cert_len - (c - cert);
        if (buf.len > c_left)
            break;
        if (memcmp(c, buf.data, buf.len) == 0) {
            match = TRUE;
            break;
        }
        assert(c_left >= 1);
        c = memchr(c + 1, *cntag, c_left - 1);
    }

    k5_buf_free(&buf);
    return match;
}

/*
 * Test module 2 returns OK if princ matches the CN part of the subject name,
 * and returns indicators of the module name and princ.
 */
static krb5_error_code
test2_authorize(krb5_context context, krb5_certauth_moddata moddata,
                const uint8_t *cert, size_t cert_len,
                krb5_const_principal princ, const void *opts,
                const struct _krb5_db_entry_new *db_entry,
                char ***authinds_out)
{
    krb5_error_code ret;
    char *name = NULL, **ais = NULL;

    *authinds_out = NULL;

    assert(moddata != NULL && moddata->initialized);

    ret = krb5_unparse_name_flags(context, princ,
                                  KRB5_PRINCIPAL_UNPARSE_NO_REALM, &name);
    if (ret)
        goto cleanup;

    if (!has_cn(context, cert, cert_len, name)) {
        ret = KRB5KDC_ERR_CERTIFICATE_MISMATCH;
        goto cleanup;
    }

    /* Create an indicator list with the module name and CN. */
    ais = calloc(3, sizeof(*ais));
    assert(ais != NULL);
    ais[0] = strdup("test2");
    ais[1] = strdup(name);
    assert(ais[0] != NULL && ais[1] != NULL);
    *authinds_out = ais;

    ais = NULL;

cleanup:
    krb5_free_unparsed_name(context, name);
    return ret;
}

krb5_error_code
certauth_test1_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable);
krb5_error_code
certauth_test1_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable)
{
    krb5_certauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_certauth_vtable)vtable;
    vt->name = "test1";
    vt->authorize = test1_authorize;
    vt->free_ind = test_free_ind;
    return 0;
}

krb5_error_code
certauth_test2_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable);
krb5_error_code
certauth_test2_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable)
{
    krb5_certauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_certauth_vtable)vtable;
    vt->name = "test2";
    vt->authorize = test2_authorize;
    vt->init = test2_init;
    vt->fini = test2_fini;
    vt->free_ind = test_free_ind;
    return 0;
}
