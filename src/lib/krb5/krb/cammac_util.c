/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* cammac_util.c - CAMMAC related functions */
/*
 * Copyright (C) 2016 by Red Hat, Inc.
 * All Rights Reserved.
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

/* Return 0 if cammac's service verifier is valid for server_key. */
static krb5_error_code
check_svcver(krb5_context context, const krb5_cammac *cammac,
             const krb5_keyblock *server_key)
{
    krb5_error_code ret;
    krb5_boolean valid = FALSE;
    krb5_verifier_mac *ver = cammac->svc_verifier;
    krb5_data *der_authdata = NULL;

    if (ver == NULL)
        return EINVAL;

    ret = encode_krb5_authdata(cammac->elements, &der_authdata);
    if (ret)
        return ret;

    ret = krb5_c_verify_checksum(context, server_key, KRB5_KEYUSAGE_CAMMAC,
                                 der_authdata, &ver->checksum, &valid);
    if (!ret && !valid)
        ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;

    krb5_free_data(context, der_authdata);
    return ret;
}

/* Decode and verify CAMMAC authdata using the svc verifier,
 * and return the contents as an allocated array of authdata pointers. */
krb5_error_code
k5_unwrap_cammac_svc(krb5_context context, const krb5_authdata *ad,
                     const krb5_keyblock *key, krb5_authdata ***adata_out)
{
    krb5_data ad_data;
    krb5_error_code ret;
    krb5_cammac *cammac = NULL;

    *adata_out = NULL;

    ad_data = make_data(ad->contents, ad->length);
    ret = decode_krb5_cammac(&ad_data, &cammac);
    if (ret)
        return ret;

    ret = check_svcver(context, cammac, key);
    if (!ret) {
        *adata_out = cammac->elements;
        cammac->elements = NULL;
    }

    k5_free_cammac(context, cammac);
    return ret;
}
