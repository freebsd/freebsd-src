/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/ser_princ.c - Serialize krb5_principal structure */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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
 * Routines to deal with externalizing the krb5_principal:
 *      krb5_principal_size();
 *      krb5_principal_externalize();
 *      krb5_principal_internalize();
 */
static krb5_error_code krb5_principal_size
(krb5_context, krb5_pointer, size_t *);
static krb5_error_code krb5_principal_externalize
(krb5_context, krb5_pointer, krb5_octet **, size_t *);
static krb5_error_code krb5_principal_internalize
(krb5_context,krb5_pointer *, krb5_octet **, size_t *);

/* Local data */
static const krb5_ser_entry krb5_principal_ser_entry = {
    KV5M_PRINCIPAL,                     /* Type                 */
    krb5_principal_size,                /* Sizer routine        */
    krb5_principal_externalize,         /* Externalize routine  */
    krb5_principal_internalize          /* Internalize routine  */
};

/*
 * krb5_principal_size()        - Determine the size required to externalize
 *                                the krb5_principal.
 */
static krb5_error_code
krb5_principal_size(krb5_context kcontext, krb5_pointer arg, size_t *sizep)
{
    krb5_error_code     kret;
    krb5_principal      principal;
    char                *fname;

    /*
     * krb5_principal requires:
     *  krb5_int32                      for KV5M_PRINCIPAL
     *  krb5_int32                      for flattened name size
     *  strlen(name)                    for name.
     *  krb5_int32                      for KV5M_PRINCIPAL
     */
    kret = EINVAL;
    if ((principal = (krb5_principal) arg) &&
        !(kret = krb5_unparse_name(kcontext, principal, &fname))) {
        *sizep += (3*sizeof(krb5_int32)) + strlen(fname);
        free(fname);
    }
    return(kret);
}

/*
 * krb5_principal_externalize() - Externalize the krb5_principal.
 */
static krb5_error_code
krb5_principal_externalize(krb5_context kcontext, krb5_pointer arg, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_principal      principal;
    size_t              required;
    krb5_octet          *bp;
    size_t              remain;
    char                *fname;

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if ((principal = (krb5_principal) arg)) {
        kret = ENOMEM;
        if (!krb5_principal_size(kcontext, arg, &required) &&
            (required <= remain)) {
            if (!(kret = krb5_unparse_name(kcontext, principal, &fname))) {

                (void) krb5_ser_pack_int32(KV5M_PRINCIPAL, &bp, &remain);
                (void) krb5_ser_pack_int32((krb5_int32) strlen(fname),
                                           &bp, &remain);
                (void) krb5_ser_pack_bytes((krb5_octet *) fname,
                                           strlen(fname), &bp, &remain);
                (void) krb5_ser_pack_int32(KV5M_PRINCIPAL, &bp, &remain);
                *buffer = bp;
                *lenremain = remain;

                free(fname);
            }
        }
    }
    return(kret);
}

/*
 * krb5_principal_internalize() - Internalize the krb5_principal.
 */
static krb5_error_code
krb5_principal_internalize(krb5_context kcontext, krb5_pointer *argp, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_principal      principal = NULL;
    krb5_int32          ibuf;
    krb5_octet          *bp;
    size_t              remain;
    char                *tmpname = NULL;

    *argp = NULL;
    bp = *buffer;
    remain = *lenremain;

    /* Read our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain) || ibuf != KV5M_PRINCIPAL)
        return EINVAL;

    /* Read the principal name */
    kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
    if (kret)
        return kret;
    tmpname = malloc(ibuf + 1);
    kret = krb5_ser_unpack_bytes((krb5_octet *) tmpname, (size_t) ibuf,
                                 &bp, &remain);
    if (kret)
        goto cleanup;
    tmpname[ibuf] = '\0';

    /* Parse the name to a principal structure */
    kret = krb5_parse_name(kcontext, tmpname, &principal);
    if (kret)
        goto cleanup;

    /* Read the trailing magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain) || ibuf != KV5M_PRINCIPAL) {
        kret = EINVAL;
        goto cleanup;
    }

    *buffer = bp;
    *lenremain = remain;
    *argp = principal;
cleanup:
    if (kret)
        krb5_free_principal(kcontext, principal);
    free(tmpname);
    return kret;
}

/*
 * Register the context serializer.
 */
krb5_error_code
krb5_ser_principal_init(krb5_context kcontext)
{
    return(krb5_register_serializer(kcontext, &krb5_principal_ser_entry));
}
