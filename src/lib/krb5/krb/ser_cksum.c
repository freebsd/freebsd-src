/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/ser_cksum.c - Serialize krb5_checksum structure */
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
 * Routines to deal with externalizing the krb5_checksum:
 *      krb5_checksum_esize();
 *      krb5_checksum_externalize();
 *      krb5_checksum_internalize();
 */
static krb5_error_code krb5_checksum_esize
(krb5_context, krb5_pointer, size_t *);
static krb5_error_code krb5_checksum_externalize
(krb5_context, krb5_pointer, krb5_octet **, size_t *);
static krb5_error_code krb5_checksum_internalize
(krb5_context,krb5_pointer *, krb5_octet **, size_t *);

/* Local data */
static const krb5_ser_entry krb5_checksum_ser_entry = {
    KV5M_CHECKSUM,                      /* Type                 */
    krb5_checksum_esize,                /* Sizer routine        */
    krb5_checksum_externalize,          /* Externalize routine  */
    krb5_checksum_internalize           /* Internalize routine  */
};

/*
 * krb5_checksum_esize()        - Determine the size required to externalize
 *                                the krb5_checksum.
 */
static krb5_error_code
krb5_checksum_esize(krb5_context kcontext, krb5_pointer arg, size_t *sizep)
{
    krb5_error_code     kret;
    krb5_checksum       *checksum;

    /*
     * krb5_checksum requires:
     *  krb5_int32              for KV5M_CHECKSUM
     *  krb5_int32              for checksum_type
     *  krb5_int32              for length
     *  krb5_int32              for KV5M_CHECKSUM
     *  checksum->length        for contents
     */
    kret = EINVAL;
    if ((checksum = (krb5_checksum *) arg)) {
        *sizep += (sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   (size_t) checksum->length);
        kret = 0;
    }
    return(kret);
}

/*
 * krb5_checksum_externalize()  - Externalize the krb5_checksum.
 */
static krb5_error_code
krb5_checksum_externalize(krb5_context kcontext, krb5_pointer arg, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_checksum       *checksum;
    size_t              required;
    krb5_octet          *bp;
    size_t              remain;

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if ((checksum = (krb5_checksum *) arg)) {
        kret = ENOMEM;
        if (!krb5_checksum_esize(kcontext, arg, &required) &&
            (required <= remain)) {
            /* Our identifier */
            (void) krb5_ser_pack_int32(KV5M_CHECKSUM, &bp, &remain);

            /* Our checksum_type */
            (void) krb5_ser_pack_int32((krb5_int32) checksum->checksum_type,
                                       &bp, &remain);

            /* Our length */
            (void) krb5_ser_pack_int32((krb5_int32) checksum->length,
                                       &bp, &remain);

            /* Our contents */
            (void) krb5_ser_pack_bytes(checksum->contents,
                                       (size_t) checksum->length,
                                       &bp, &remain);

            /* Finally, our trailer */
            (void) krb5_ser_pack_int32(KV5M_CHECKSUM, &bp, &remain);

            kret = 0;
            *buffer = bp;
            *lenremain = remain;
        }
    }
    return(kret);
}

/*
 * krb5_checksum_internalize()  - Internalize the krb5_checksum.
 */
static krb5_error_code
krb5_checksum_internalize(krb5_context kcontext, krb5_pointer *argp, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_checksum       *checksum;
    krb5_int32          ibuf;
    krb5_octet          *bp;
    size_t              remain;

    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    /* Read our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain))
        ibuf = 0;
    if (ibuf == KV5M_CHECKSUM) {
        kret = ENOMEM;

        /* Get a checksum */
        if ((remain >= (2*sizeof(krb5_int32))) &&
            (checksum = (krb5_checksum *) calloc(1, sizeof(krb5_checksum)))) {
            /* Get the checksum_type */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            checksum->checksum_type = (krb5_cksumtype) ibuf;

            /* Get the length */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            checksum->length = (int) ibuf;

            /* Get the string */
            if (!ibuf ||
                ((checksum->contents = (krb5_octet *)
                  malloc((size_t) (ibuf))) &&
                 !(kret = krb5_ser_unpack_bytes(checksum->contents,
                                                (size_t) ibuf,
                                                &bp, &remain)))) {

                /* Get the trailer */
                kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
                if (!kret && (ibuf == KV5M_CHECKSUM)) {
                    checksum->magic = KV5M_CHECKSUM;
                    *buffer = bp;
                    *lenremain = remain;
                    *argp = (krb5_pointer) checksum;
                }
                else
                    kret = EINVAL;
            }
            if (kret) {
                if (checksum->contents)
                    free(checksum->contents);
                free(checksum);
            }
        }
    }
    return(kret);
}

/*
 * Register the checksum serializer.
 */
krb5_error_code
krb5_ser_checksum_init(krb5_context kcontext)
{
    return(krb5_register_serializer(kcontext, &krb5_checksum_ser_entry));
}
