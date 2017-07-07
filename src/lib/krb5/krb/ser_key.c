/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/ser_key.c - Serialize krb5_keyblock structure */
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
 * Routines to deal with externalizing the krb5_keyblock:
 *      krb5_keyblock_size();
 *      krb5_keyblock_externalize();
 *      krb5_keyblock_internalize();
 */
static krb5_error_code krb5_keyblock_size
(krb5_context, krb5_pointer, size_t *);
static krb5_error_code krb5_keyblock_externalize
(krb5_context, krb5_pointer, krb5_octet **, size_t *);
static krb5_error_code krb5_keyblock_internalize
(krb5_context,krb5_pointer *, krb5_octet **, size_t *);

/* Local data */
static const krb5_ser_entry krb5_keyblock_ser_entry = {
    KV5M_KEYBLOCK,                      /* Type                 */
    krb5_keyblock_size,                 /* Sizer routine        */
    krb5_keyblock_externalize,          /* Externalize routine  */
    krb5_keyblock_internalize           /* Internalize routine  */
};

/*
 * krb5_keyblock_size() - Determine the size required to externalize
 *                                the krb5_keyblock.
 */
static krb5_error_code
krb5_keyblock_size(krb5_context kcontext, krb5_pointer arg, size_t *sizep)
{
    krb5_error_code     kret;
    krb5_keyblock       *keyblock;

    /*
     * krb5_keyblock requires:
     *  krb5_int32                      for KV5M_KEYBLOCK
     *  krb5_int32                      for enctype
     *  krb5_int32                      for length
     *  keyblock->length                for contents
     *  krb5_int32                      for KV5M_KEYBLOCK
     */
    kret = EINVAL;
    if ((keyblock = (krb5_keyblock *) arg)) {
        *sizep += (sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   sizeof(krb5_int32) +
                   (size_t) keyblock->length);
        kret = 0;
    }
    return(kret);
}

/*
 * krb5_keyblock_externalize()  - Externalize the krb5_keyblock.
 */
static krb5_error_code
krb5_keyblock_externalize(krb5_context kcontext, krb5_pointer arg, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_keyblock       *keyblock;
    size_t              required;
    krb5_octet          *bp;
    size_t              remain;

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if ((keyblock = (krb5_keyblock *) arg)) {
        kret = ENOMEM;
        if (!krb5_keyblock_size(kcontext, arg, &required) &&
            (required <= remain)) {
            /* Our identifier */
            (void) krb5_ser_pack_int32(KV5M_KEYBLOCK, &bp, &remain);

            /* Our enctype */
            (void) krb5_ser_pack_int32((krb5_int32) keyblock->enctype,
                                       &bp, &remain);

            /* Our length */
            (void) krb5_ser_pack_int32((krb5_int32) keyblock->length,
                                       &bp, &remain);

            /* Our contents */
            (void) krb5_ser_pack_bytes(keyblock->contents,
                                       (size_t) keyblock->length,
                                       &bp, &remain);

            /* Finally, our trailer */
            (void) krb5_ser_pack_int32(KV5M_KEYBLOCK, &bp, &remain);

            kret = 0;
            *buffer = bp;
            *lenremain = remain;
        }
    }
    return(kret);
}

/*
 * krb5_keyblock_internalize()  - Internalize the krb5_keyblock.
 */
static krb5_error_code
krb5_keyblock_internalize(krb5_context kcontext, krb5_pointer *argp, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_keyblock       *keyblock;
    krb5_int32          ibuf;
    krb5_octet          *bp;
    size_t              remain;

    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    /* Read our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain))
        ibuf = 0;
    if (ibuf == KV5M_KEYBLOCK) {
        kret = ENOMEM;

        /* Get a keyblock */
        if ((remain >= (3*sizeof(krb5_int32))) &&
            (keyblock = (krb5_keyblock *) calloc(1, sizeof(krb5_keyblock)))) {
            /* Get the enctype */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            keyblock->enctype = (krb5_enctype) ibuf;

            /* Get the length */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            keyblock->length = (int) ibuf;

            /* Get the string */
            if ((keyblock->contents = (krb5_octet *) malloc((size_t) (ibuf)))&&
                !(kret = krb5_ser_unpack_bytes(keyblock->contents,
                                               (size_t) ibuf,
                                               &bp, &remain))) {
                kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
                if (!kret && (ibuf == KV5M_KEYBLOCK)) {
                    kret = 0;
                    *buffer = bp;
                    *lenremain = remain;
                    keyblock->magic = KV5M_KEYBLOCK;
                    *argp = (krb5_pointer) keyblock;
                }
                else
                    kret = EINVAL;
            }
            if (kret) {
                if (keyblock->contents)
                    free(keyblock->contents);
                free(keyblock);
            }
        }
    }
    return(kret);
}

/*
 * Register the keyblock serializer.
 */
krb5_error_code
krb5_ser_keyblock_init(krb5_context kcontext)
{
    return(krb5_register_serializer(kcontext, &krb5_keyblock_ser_entry));
}
