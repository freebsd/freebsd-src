/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/ser_auth.c - Serialize krb5_authenticator structure */
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

#ifndef LEAN_CLIENT

#include "k5-int.h"
#include "int-proto.h"

/*
 * Routines to deal with externalizing the krb5_authenticator:
 *      krb5_authenticator_size();
 *      krb5_authenticator_externalize();
 *      krb5_authenticator_internalize();
 */
static krb5_error_code krb5_authenticator_size
(krb5_context, krb5_pointer, size_t *);
static krb5_error_code krb5_authenticator_externalize
(krb5_context, krb5_pointer, krb5_octet **, size_t *);
static krb5_error_code krb5_authenticator_internalize
(krb5_context,krb5_pointer *, krb5_octet **, size_t *);

/* Local data */
static const krb5_ser_entry krb5_authenticator_ser_entry = {
    KV5M_AUTHENTICATOR,                 /* Type                 */
    krb5_authenticator_size,            /* Sizer routine        */
    krb5_authenticator_externalize,     /* Externalize routine  */
    krb5_authenticator_internalize      /* Internalize routine  */
};

/*
 * krb5_authenticator_size()    - Determine the size required to externalize
 *                                the krb5_authenticator.
 */
static krb5_error_code
krb5_authenticator_size(krb5_context kcontext, krb5_pointer arg, size_t *sizep)
{
    krb5_error_code     kret;
    krb5_authenticator  *authenticator;
    size_t              required;

    /*
     * krb5_authenticator requires at minimum:
     *  krb5_int32              for KV5M_AUTHENTICATOR
     *  krb5_int32              for seconds
     *  krb5_int32              for cusec
     *  krb5_int32              for seq_number
     *  krb5_int32              for number in authorization_data array.
     *  krb5_int32              for KV5M_AUTHENTICATOR
     */
    kret = EINVAL;
    if ((authenticator = (krb5_authenticator *) arg)) {
        required = sizeof(krb5_int32)*6;

        /* Calculate size required by client, if appropriate */
        if (authenticator->client)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_PRINCIPAL,
                                    (krb5_pointer) authenticator->client,
                                    &required);
        else
            kret = 0;

        /* Calculate size required by checksum, if appropriate */
        if (!kret && authenticator->checksum)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_CHECKSUM,
                                    (krb5_pointer) authenticator->checksum,
                                    &required);

        /* Calculate size required by subkey, if appropriate */
        if (!kret && authenticator->subkey)
            kret = krb5_size_opaque(kcontext,
                                    KV5M_KEYBLOCK,
                                    (krb5_pointer) authenticator->subkey,
                                    &required);

        /* Calculate size required by authorization_data, if appropriate */
        if (!kret && authenticator->authorization_data) {
            int i;

            for (i=0; !kret && authenticator->authorization_data[i]; i++) {
                kret = krb5_size_opaque(kcontext,
                                        KV5M_AUTHDATA,
                                        (krb5_pointer) authenticator->
                                        authorization_data[i],
                                        &required);
            }
        }
    }
    if (!kret)
        *sizep += required;
    return(kret);
}

/*
 * krb5_authenticator_externalize()     - Externalize the krb5_authenticator.
 */
static krb5_error_code
krb5_authenticator_externalize(krb5_context kcontext, krb5_pointer arg, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_authenticator  *authenticator;
    size_t              required;
    krb5_octet          *bp;
    size_t              remain;
    int                 i;

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if ((authenticator = (krb5_authenticator *) arg)) {
        kret = ENOMEM;
        if (!krb5_authenticator_size(kcontext, arg, &required) &&
            (required <= remain)) {
            /* First write our magic number */
            (void) krb5_ser_pack_int32(KV5M_AUTHENTICATOR, &bp, &remain);

            /* Now ctime */
            (void) krb5_ser_pack_int32((krb5_int32) authenticator->ctime,
                                       &bp, &remain);

            /* Now cusec */
            (void) krb5_ser_pack_int32((krb5_int32) authenticator->cusec,
                                       &bp, &remain);

            /* Now seq_number */
            (void) krb5_ser_pack_int32(authenticator->seq_number,
                                       &bp, &remain);

            /* Now handle client, if appropriate */
            if (authenticator->client)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_PRINCIPAL,
                                               (krb5_pointer)
                                               authenticator->client,
                                               &bp,
                                               &remain);
            else
                kret = 0;

            /* Now handle checksum, if appropriate */
            if (!kret && authenticator->checksum)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_CHECKSUM,
                                               (krb5_pointer)
                                               authenticator->checksum,
                                               &bp,
                                               &remain);

            /* Now handle subkey, if appropriate */
            if (!kret && authenticator->subkey)
                kret = krb5_externalize_opaque(kcontext,
                                               KV5M_KEYBLOCK,
                                               (krb5_pointer)
                                               authenticator->subkey,
                                               &bp,
                                               &remain);

            /* Now handle authorization_data, if appropriate */
            if (!kret) {
                if (authenticator->authorization_data)
                    for (i=0; authenticator->authorization_data[i]; i++);
                else
                    i = 0;
                (void) krb5_ser_pack_int32((krb5_int32) i, &bp, &remain);

                /* Now pound out the authorization_data */
                if (authenticator->authorization_data) {
                    for (i=0; !kret && authenticator->authorization_data[i];
                         i++)
                        kret = krb5_externalize_opaque(kcontext,
                                                       KV5M_AUTHDATA,
                                                       (krb5_pointer)
                                                       authenticator->
                                                       authorization_data[i],
                                                       &bp,
                                                       &remain);
                }
            }

            /*
             * If we were successful, write trailer then update the pointer and
             * remaining length;
             */
            if (!kret) {
                /* Write our trailer */
                (void) krb5_ser_pack_int32(KV5M_AUTHENTICATOR, &bp, &remain);
                *buffer = bp;
                *lenremain = remain;
            }
        }
    }
    return(kret);
}

/*
 * krb5_authenticator_internalize()     - Internalize the krb5_authenticator.
 */
static krb5_error_code
krb5_authenticator_internalize(krb5_context kcontext, krb5_pointer *argp, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_authenticator  *authenticator;
    krb5_int32          ibuf;
    krb5_octet          *bp;
    size_t              remain;
    int                 i;
    krb5_int32          nadata;
    size_t              len;

    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    /* Read our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain))
        ibuf = 0;
    if (ibuf == KV5M_AUTHENTICATOR) {
        kret = ENOMEM;

        /* Get memory for the authenticator */
        if ((remain >= (3*sizeof(krb5_int32))) &&
            (authenticator = (krb5_authenticator *)
             calloc(1, sizeof(krb5_authenticator)))) {

            /* Get ctime */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            authenticator->ctime = (krb5_timestamp) ibuf;

            /* Get cusec */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            authenticator->cusec = ibuf;

            /* Get seq_number */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            authenticator->seq_number = ibuf;

            kret = 0;

            /* Attempt to read in the client */
            kret = krb5_internalize_opaque(kcontext,
                                           KV5M_PRINCIPAL,
                                           (krb5_pointer *)
                                           &authenticator->client,
                                           &bp,
                                           &remain);
            if (kret == EINVAL)
                kret = 0;

            /* Attempt to read in the checksum */
            if (!kret) {
                kret = krb5_internalize_opaque(kcontext,
                                               KV5M_CHECKSUM,
                                               (krb5_pointer *)
                                               &authenticator->checksum,
                                               &bp,
                                               &remain);
                if (kret == EINVAL)
                    kret = 0;
            }

            /* Attempt to read in the subkey */
            if (!kret) {
                kret = krb5_internalize_opaque(kcontext,
                                               KV5M_KEYBLOCK,
                                               (krb5_pointer *)
                                               &authenticator->subkey,
                                               &bp,
                                               &remain);
                if (kret == EINVAL)
                    kret = 0;
            }

            /* Attempt to read in the authorization data count */
            if (!(kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain))) {
                nadata = ibuf;
                len = (size_t) (nadata + 1);

                /* Get memory for the authorization data pointers */
                if ((authenticator->authorization_data = (krb5_authdata **)
                     calloc(len, sizeof(krb5_authdata *)))) {
                    for (i=0; !kret && (i<nadata); i++) {
                        kret = krb5_internalize_opaque(kcontext,
                                                       KV5M_AUTHDATA,
                                                       (krb5_pointer *)
                                                       &authenticator->
                                                       authorization_data[i],
                                                       &bp,
                                                       &remain);
                    }

                    /* Finally, find the trailer */
                    if (!kret) {
                        kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
                        if (!kret && (ibuf == KV5M_AUTHENTICATOR))
                            authenticator->magic = KV5M_AUTHENTICATOR;
                        else
                            kret = EINVAL;
                    }
                }
            }
            if (!kret) {
                *buffer = bp;
                *lenremain = remain;
                *argp = (krb5_pointer) authenticator;
            }
            else
                krb5_free_authenticator(kcontext, authenticator);
        }
    }
    return(kret);
}
/*
 * Register the authenticator serializer.
 */
krb5_error_code
krb5_ser_authenticator_init(krb5_context kcontext)
{
    return(krb5_register_serializer(kcontext, &krb5_authenticator_ser_entry));
}
#endif
