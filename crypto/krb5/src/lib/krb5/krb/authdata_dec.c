/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/authdata_dec.c */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */
/*
 * Copyright (c) 2006-2008, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "int-proto.h"

krb5_error_code KRB5_CALLCONV
krb5_decode_authdata_container(krb5_context context,
                               krb5_authdatatype type,
                               const krb5_authdata *container,
                               krb5_authdata ***authdata)
{
    krb5_error_code code;
    krb5_data data;

    *authdata = NULL;

    if ((container->ad_type & AD_TYPE_FIELD_TYPE_MASK) != type)
        return EINVAL;

    data.length = container->length;
    data.data = (char *)container->contents;

    code = decode_krb5_authdata(&data, authdata);
    if (code)
        return code;

    return 0;
}

struct find_authdata_context {
    krb5_authdata **out;
    size_t space;
    size_t length;
};

static krb5_error_code
grow_find_authdata(krb5_context context, struct find_authdata_context *fctx,
                   krb5_authdata *elem)
{
    krb5_error_code retval = 0;
    if (fctx->length == fctx->space) {
        krb5_authdata **new;
        if (fctx->space >= 256) {
            k5_setmsg(context, ERANGE,
                      "More than 256 authdata matched a query");
            return ERANGE;
        }
        new       = realloc(fctx->out,
                            sizeof (krb5_authdata *)*(2*fctx->space+1));
        if (new == NULL)
            return ENOMEM;
        fctx->out = new;
        fctx->space *=2;
    }
    fctx->out[fctx->length+1] = NULL;
    retval = krb5int_copy_authdatum(context, elem,
                                    &fctx->out[fctx->length]);
    if (retval == 0)
        fctx->length++;
    return retval;
}

static krb5_error_code
find_authdata_1(krb5_context context, krb5_authdata *const *in_authdat,
                krb5_authdatatype ad_type, struct find_authdata_context *fctx,
                int from_ap_req)
{
    int i = 0;
    krb5_error_code retval = 0;

    for (i = 0; in_authdat[i] && retval == 0; i++) {
        krb5_authdata *ad = in_authdat[i];
        krb5_authdata **decoded_container;

        switch (ad->ad_type) {
        case KRB5_AUTHDATA_IF_RELEVANT:
            if (retval == 0)
                retval = krb5_decode_authdata_container(context,
                                                        ad->ad_type,
                                                        ad,
                                                        &decoded_container);
            if (retval == 0) {
                retval = find_authdata_1(context,
                                         decoded_container,
                                         ad_type,
                                         fctx,
                                         from_ap_req);
                krb5_free_authdata(context, decoded_container);
            }
            break;
        case KRB5_AUTHDATA_SIGNTICKET:
        case KRB5_AUTHDATA_KDC_ISSUED:
        case KRB5_AUTHDATA_WIN2K_PAC:
        case KRB5_AUTHDATA_CAMMAC:
        case KRB5_AUTHDATA_AUTH_INDICATOR:
            if (from_ap_req)
                continue;
        default:
            if (ad->ad_type == ad_type && retval == 0)
                retval = grow_find_authdata(context, fctx, ad);
            break;
        }
    }

    return retval;
}

krb5_error_code KRB5_CALLCONV
krb5_find_authdata(krb5_context context,
                   krb5_authdata *const *ticket_authdata,
                   krb5_authdata *const *ap_req_authdata,
                   krb5_authdatatype ad_type, krb5_authdata ***results)
{
    krb5_error_code retval = 0;
    struct find_authdata_context fctx;
    fctx.length = 0;
    fctx.space = 2;
    fctx.out = calloc(fctx.space+1, sizeof (krb5_authdata *));
    *results = NULL;
    if (fctx.out == NULL)
        return ENOMEM;
    if (ticket_authdata)
        retval = find_authdata_1( context, ticket_authdata, ad_type, &fctx, 0);
    if ((retval==0) && ap_req_authdata)
        retval = find_authdata_1( context, ap_req_authdata, ad_type, &fctx, 1);
    if ((retval== 0) && fctx.length)
        *results = fctx.out;
    else krb5_free_authdata(context, fctx.out);
    return retval;
}

krb5_error_code KRB5_CALLCONV
krb5_verify_authdata_kdc_issued(krb5_context context,
                                const krb5_keyblock *key,
                                const krb5_authdata *ad_kdcissued,
                                krb5_principal *issuer,
                                krb5_authdata ***authdata)
{
    krb5_error_code code;
    krb5_ad_kdcissued *ad_kdci;
    krb5_data data, *data2;
    krb5_boolean valid = FALSE;

    if ((ad_kdcissued->ad_type & AD_TYPE_FIELD_TYPE_MASK) !=
        KRB5_AUTHDATA_KDC_ISSUED)
        return EINVAL;

    if (issuer != NULL)
        *issuer = NULL;
    if (authdata != NULL)
        *authdata = NULL;

    data.length = ad_kdcissued->length;
    data.data = (char *)ad_kdcissued->contents;

    code = decode_krb5_ad_kdcissued(&data, &ad_kdci);
    if (code != 0)
        return code;

    if (!krb5_c_is_keyed_cksum(ad_kdci->ad_checksum.checksum_type)) {
        krb5_free_ad_kdcissued(context, ad_kdci);
        return KRB5KRB_AP_ERR_INAPP_CKSUM;
    }

    code = encode_krb5_authdata(ad_kdci->elements, &data2);
    if (code != 0) {
        krb5_free_ad_kdcissued(context, ad_kdci);
        return code;
    }

    code = krb5_c_verify_checksum(context, key,
                                  KRB5_KEYUSAGE_AD_KDCISSUED_CKSUM,
                                  data2, &ad_kdci->ad_checksum, &valid);
    if (code != 0) {
        krb5_free_ad_kdcissued(context, ad_kdci);
        krb5_free_data(context, data2);
        return code;
    }

    krb5_free_data(context, data2);

    if (valid == FALSE) {
        krb5_free_ad_kdcissued(context, ad_kdci);
        return KRB5KRB_AP_ERR_BAD_INTEGRITY;
    }

    if (issuer != NULL) {
        *issuer = ad_kdci->i_principal;
        ad_kdci->i_principal = NULL;
    }

    if (authdata != NULL) {
        *authdata = ad_kdci->elements;
        ad_kdci->elements = NULL;
    }

    krb5_free_ad_kdcissued(context, ad_kdci);

    return 0;
}

/*
 * Decode authentication indicator strings from authdata and return as an
 * allocated array of krb5_data pointers.  The caller must initialize
 * *indicators to NULL for the first call, and successive calls will reallocate
 * and append to the indicators array.
 */
krb5_error_code
k5_authind_decode(const krb5_authdata *ad, krb5_data ***indicators)
{
    krb5_error_code ret = 0;
    krb5_data der_ad, **strdata = NULL, **ai_list = *indicators;
    size_t count, scount;

    if (ad == NULL || ad->ad_type != KRB5_AUTHDATA_AUTH_INDICATOR)
        goto cleanup;

    /* Count existing auth indicators. */
    for (count = 0; ai_list != NULL && ai_list[count] != NULL; count++);

    der_ad = make_data(ad->contents, ad->length);
    ret = decode_utf8_strings(&der_ad, &strdata);
    if (ret)
        return ret;

    /* Count new auth indicators. */
    for (scount = 0; strdata[scount] != NULL; scount++);

    ai_list = realloc(ai_list, (count + scount + 1) * sizeof(*ai_list));
    if (ai_list == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }
    *indicators = ai_list;

    /* Steal decoder-allocated pointers and free the container array. */
    memcpy(ai_list + count, strdata, scount * sizeof(*strdata));
    ai_list[count + scount] = NULL;
    free(strdata);
    strdata = NULL;

cleanup:
    k5_free_data_ptr_list(strdata);
    return ret;
}
