/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/authdata_enc.c */
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

krb5_error_code KRB5_CALLCONV
krb5_encode_authdata_container(krb5_context context,
                               krb5_authdatatype type,
                               krb5_authdata *const*authdata,
                               krb5_authdata ***container)
{
    krb5_error_code code;
    krb5_data *data;
    krb5_authdata ad_datum;
    krb5_authdata *ad_data[2];

    *container = NULL;

    code = encode_krb5_authdata((krb5_authdata * const *)authdata, &data);
    if (code)
        return code;

    ad_datum.ad_type = type & AD_TYPE_FIELD_TYPE_MASK;
    ad_datum.length = data->length;
    ad_datum.contents = (unsigned char *)data->data;

    ad_data[0] = &ad_datum;
    ad_data[1] = NULL;

    code = krb5_copy_authdata(context, ad_data, container);

    krb5_free_data(context, data);

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_make_authdata_kdc_issued(krb5_context context,
                              const krb5_keyblock *key,
                              krb5_const_principal issuer,
                              krb5_authdata *const *authdata,
                              krb5_authdata ***ad_kdcissued)
{
    krb5_error_code code;
    krb5_ad_kdcissued ad_kdci;
    krb5_data *data;
    krb5_cksumtype cksumtype;
    krb5_authdata ad_datum;
    krb5_authdata *ad_data[2];

    *ad_kdcissued = NULL;

    ad_kdci.ad_checksum.contents = NULL;
    ad_kdci.i_principal = (krb5_principal)issuer;
    ad_kdci.elements = (krb5_authdata **)authdata;

    code = krb5int_c_mandatory_cksumtype(context, key->enctype,
                                         &cksumtype);
    if (code != 0)
        return code;

    if (!krb5_c_is_keyed_cksum(cksumtype))
        return KRB5KRB_AP_ERR_INAPP_CKSUM;

    code = encode_krb5_authdata(ad_kdci.elements, &data);
    if (code != 0)
        return code;

    code = krb5_c_make_checksum(context, cksumtype,
                                key, KRB5_KEYUSAGE_AD_KDCISSUED_CKSUM,
                                data, &ad_kdci.ad_checksum);
    if (code != 0) {
        krb5_free_data(context, data);
        return code;
    }

    krb5_free_data(context, data);

    code = encode_krb5_ad_kdcissued(&ad_kdci, &data);
    if (code != 0)
        return code;

    ad_datum.ad_type = KRB5_AUTHDATA_KDC_ISSUED;
    ad_datum.length = data->length;
    ad_datum.contents = (unsigned char *)data->data;

    ad_data[0] = &ad_datum;
    ad_data[1] = NULL;

    code = krb5_copy_authdata(context, ad_data, ad_kdcissued);

    krb5_free_data(context, data);
    krb5_free_checksum_contents(context, &ad_kdci.ad_checksum);

    return code;
}
