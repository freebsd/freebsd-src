/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/ser_adata.c - Serialize krb5_authdata structure */
/*
 * Copyright 1995, 2008 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "int-proto.h"

/*
 * Routines to deal with externalizing the krb5_authdata:
 *      krb5_authdata_size();
 *      krb5_authdata_externalize();
 *      krb5_authdata_internalize();
 */
static krb5_error_code krb5_authdata_size
(krb5_context, krb5_pointer, size_t *);
static krb5_error_code krb5_authdata_externalize
(krb5_context, krb5_pointer, krb5_octet **, size_t *);
static krb5_error_code krb5_authdata_internalize
(krb5_context,krb5_pointer *, krb5_octet **, size_t *);

/* Local data */
static const krb5_ser_entry krb5_authdata_ser_entry = {
    KV5M_AUTHDATA,                      /* Type                 */
    krb5_authdata_size,                 /* Sizer routine        */
    krb5_authdata_externalize,          /* Externalize routine  */
    krb5_authdata_internalize           /* Internalize routine  */
};

/*
 * krb5_authdata_esize()        - Determine the size required to externalize
 *                                the krb5_authdata.
 */
static krb5_error_code
krb5_authdata_size(krb5_context kcontext, krb5_pointer arg, size_t *sizep)
{
    krb5_error_code     kret;
    krb5_authdata       *authdata;

    /*
     * krb5_authdata requires:
     *  krb5_int32              for KV5M_AUTHDATA
     *  krb5_int32              for ad_type
     *  krb5_int32              for length
     *  authdata->length        for contents
     *  krb5_int32              for KV5M_AUTHDATA
     */
    kret = EINVAL;
    if ((authdata = (krb5_authdata *) arg)) {
        *sizep += (sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   (size_t) authdata->length);
        kret = 0;
    }
    return(kret);
}

/*
 * krb5_authdata_externalize()  - Externalize the krb5_authdata.
 */
static krb5_error_code
krb5_authdata_externalize(krb5_context kcontext, krb5_pointer arg, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_authdata       *authdata;
    size_t              required;
    krb5_octet          *bp;
    size_t              remain;

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if ((authdata = (krb5_authdata *) arg)) {
        kret = ENOMEM;
        if (!krb5_authdata_size(kcontext, arg, &required) &&
            (required <= remain)) {
            /* Our identifier */
            (void) krb5_ser_pack_int32(KV5M_AUTHDATA, &bp, &remain);

            /* Our ad_type */
            (void) krb5_ser_pack_int32((krb5_int32) authdata->ad_type,
                                       &bp, &remain);

            /* Our length */
            (void) krb5_ser_pack_int32((krb5_int32) authdata->length,
                                       &bp, &remain);

            /* Our contents */
            (void) krb5_ser_pack_bytes(authdata->contents,
                                       (size_t) authdata->length,
                                       &bp, &remain);

            /* Finally, our trailer */
            (void) krb5_ser_pack_int32(KV5M_AUTHDATA, &bp, &remain);
            kret = 0;
            *buffer = bp;
            *lenremain = remain;
        }
    }
    return(kret);
}

/*
 * krb5_authdata_internalize()  - Internalize the krb5_authdata.
 */
static krb5_error_code
krb5_authdata_internalize(krb5_context kcontext, krb5_pointer *argp, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_authdata       *authdata;
    krb5_int32          ibuf;
    krb5_octet          *bp;
    size_t              remain;

    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    /* Read our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain))
        ibuf = 0;
    if (ibuf == KV5M_AUTHDATA) {
        kret = ENOMEM;

        /* Get a authdata */
        if ((remain >= (2*sizeof(krb5_int32))) &&
            (authdata = (krb5_authdata *) calloc(1, sizeof(krb5_authdata)))) {

            /* Get the ad_type */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            authdata->ad_type = (krb5_authdatatype) ibuf;

            /* Get the length */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            authdata->length = (int) ibuf;

            /* Get the string */
            if ((authdata->contents = (krb5_octet *)
                 malloc((size_t) (ibuf))) &&
                !(kret = krb5_ser_unpack_bytes(authdata->contents,
                                               (size_t) ibuf,
                                               &bp, &remain))) {
                if ((kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain)))
                    ibuf = 0;
                if (ibuf == KV5M_AUTHDATA) {
                    authdata->magic = KV5M_AUTHDATA;
                    *buffer = bp;
                    *lenremain = remain;
                    *argp = (krb5_pointer) authdata;
                }
                else
                    kret = EINVAL;
            }
            if (kret) {
                if (authdata->contents)
                    free(authdata->contents);
                free(authdata);
            }
        }
    }
    return(kret);
}

/*
 * Register the authdata serializer.
 */
krb5_error_code
krb5_ser_authdata_init(krb5_context kcontext)
{
    return(krb5_register_serializer(kcontext, &krb5_authdata_ser_entry));
}
