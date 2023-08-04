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

krb5_error_code
k5_size_authenticator(krb5_authenticator *authenticator, size_t *sizep)
{
    krb5_error_code     kret;
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
    if (authenticator != NULL) {
        required = sizeof(krb5_int32)*6;

        /* Calculate size required by client, if appropriate */
        if (authenticator->client)
            kret = k5_size_principal(authenticator->client, &required);
        else
            kret = 0;

        /* Calculate size required by checksum, if appropriate */
        if (!kret && authenticator->checksum)
            kret = k5_size_checksum(authenticator->checksum, &required);

        /* Calculate size required by subkey, if appropriate */
        if (!kret && authenticator->subkey)
            kret = k5_size_keyblock(authenticator->subkey, &required);

        /* Calculate size required by authorization_data, if appropriate */
        if (!kret && authenticator->authorization_data) {
            int i;

            for (i=0; !kret && authenticator->authorization_data[i]; i++) {
                kret = k5_size_authdata(authenticator->authorization_data[i],
                                        &required);
            }
        }
    }
    if (!kret)
        *sizep += required;
    return(kret);
}

krb5_error_code
k5_externalize_authenticator(krb5_authenticator *authenticator,
                             krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    size_t              required;
    krb5_octet          *bp;
    size_t              remain;
    int                 i;

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if (authenticator != NULL) {
        kret = ENOMEM;
        if (!k5_size_authenticator(authenticator, &required) &&
            required <= remain) {
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
                kret = k5_externalize_principal(authenticator->client,
                                                &bp, &remain);
            else
                kret = 0;

            /* Now handle checksum, if appropriate */
            if (!kret && authenticator->checksum)
                kret = k5_externalize_checksum(authenticator->checksum,
                                               &bp, &remain);

            /* Now handle subkey, if appropriate */
            if (!kret && authenticator->subkey)
                kret = k5_externalize_keyblock(authenticator->subkey,
                                               &bp, &remain);

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
                        kret = k5_externalize_authdata(authenticator->
                                                       authorization_data[i],
                                                       &bp, &remain);
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

krb5_error_code
k5_internalize_authenticator(krb5_authenticator **argp,
                             krb5_octet **buffer, size_t *lenremain)
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
            kret = k5_internalize_principal(&authenticator->client,
                                            &bp, &remain);
            if (kret == EINVAL)
                kret = 0;

            /* Attempt to read in the checksum */
            if (!kret) {
                kret = k5_internalize_checksum(&authenticator->checksum,
                                               &bp, &remain);
                if (kret == EINVAL)
                    kret = 0;
            }

            /* Attempt to read in the subkey */
            if (!kret) {
                kret = k5_internalize_keyblock(&authenticator->subkey,
                                               &bp, &remain);
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
                        kret = k5_internalize_authdata(&authenticator->
                                                       authorization_data[i],
                                                       &bp, &remain);
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
                *argp = authenticator;
            }
            else
                krb5_free_authenticator(NULL, authenticator);
        }
    }
    return(kret);
}

#endif
