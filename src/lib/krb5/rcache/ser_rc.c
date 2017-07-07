/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/ser_rc.c - Serialize replay cache context */
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
#include "rc-int.h"

/*
 * Routines to deal with externalizing krb5_rcache.
 *      krb5_rcache_size();
 *      krb5_rcache_externalize();
 *      krb5_rcache_internalize();
 */
static krb5_error_code
krb5_rcache_size(krb5_context, krb5_pointer, size_t *);

static krb5_error_code
krb5_rcache_externalize(krb5_context, krb5_pointer, krb5_octet **, size_t *);

static krb5_error_code
krb5_rcache_internalize(krb5_context,krb5_pointer *, krb5_octet **, size_t *);

/*
 * Serialization entry for this type.
 */
static const krb5_ser_entry krb5_rcache_ser_entry = {
    KV5M_RCACHE,                        /* Type                 */
    krb5_rcache_size,                   /* Sizer routine        */
    krb5_rcache_externalize,            /* Externalize routine  */
    krb5_rcache_internalize             /* Internalize routine  */
};

/*
 * krb5_rcache_size()   - Determine the size required to externalize
 *                                this krb5_rcache variant.
 */
static krb5_error_code
krb5_rcache_size(krb5_context kcontext, krb5_pointer arg, size_t *sizep)
{
    krb5_error_code     kret;
    krb5_rcache         rcache;
    size_t              required;

    kret = EINVAL;
    if ((rcache = (krb5_rcache) arg)) {
        /*
         * Saving FILE: variants of krb5_rcache requires at minimum:
         *      krb5_int32      for KV5M_RCACHE
         *      krb5_int32      for length of rcache name.
         *      krb5_int32      for KV5M_RCACHE
         */
        required = sizeof(krb5_int32) * 3;
        if (rcache->ops && rcache->ops->type)
            required += (strlen(rcache->ops->type)+1);

        /*
         * The rcache name is formed as follows:
         *      <type>:<name>
         */
        required += strlen(krb5_rc_get_name(kcontext, rcache));

        kret = 0;
        *sizep += required;
    }
    return(kret);
}

/*
 * krb5_rcache_externalize()    - Externalize the krb5_rcache.
 */
static krb5_error_code
krb5_rcache_externalize(krb5_context kcontext, krb5_pointer arg, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_rcache         rcache;
    size_t              required;
    krb5_octet          *bp;
    size_t              remain;
    char                *rcname;
    char                *fnamep;

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if ((rcache = (krb5_rcache) arg)) {
        kret = ENOMEM;
        if (!krb5_rcache_size(kcontext, arg, &required) &&
            (required <= remain)) {
            /* Our identifier */
            (void) krb5_ser_pack_int32(KV5M_RCACHE, &bp, &remain);

            fnamep = krb5_rc_get_name(kcontext, rcache);

            if (rcache->ops->type) {
                if (asprintf(&rcname, "%s:%s", rcache->ops->type, fnamep) < 0)
                    rcname = NULL;
            } else
                rcname = strdup(fnamep);

            if (rcname) {
                /* Put the length of the file name */
                (void) krb5_ser_pack_int32((krb5_int32) strlen(rcname),
                                           &bp, &remain);

                /* Put the name */
                (void) krb5_ser_pack_bytes((krb5_octet *) rcname,
                                           strlen(rcname),
                                           &bp, &remain);

                /* Put the trailer */
                (void) krb5_ser_pack_int32(KV5M_RCACHE, &bp, &remain);
                kret = 0;
                *buffer = bp;
                *lenremain = remain;
                free(rcname);
            }
        }
    }
    return(kret);
}

/*
 * krb5_rcache_internalize()    - Internalize the krb5_rcache.
 */
static krb5_error_code
krb5_rcache_internalize(krb5_context kcontext, krb5_pointer *argp, krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_rcache         rcache = NULL;
    krb5_int32          ibuf;
    krb5_octet          *bp;
    size_t              remain;
    char                *rcname = NULL;

    bp = *buffer;
    remain = *lenremain;

    /* Read our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain) || ibuf != KV5M_RCACHE)
        return EINVAL;

    /* Get the length of the rcache name */
    kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
    if (kret)
        return kret;

    /* Get the rcache name. */
    rcname = malloc(ibuf + 1);
    if (!rcname)
        return ENOMEM;
    kret = krb5_ser_unpack_bytes((krb5_octet*)rcname, (size_t) ibuf,
                                 &bp, &remain);
    if (kret)
        goto cleanup;
    rcname[ibuf] = '\0';

    /* Resolve and recover the rcache. */
    kret = krb5_rc_resolve_full(kcontext, &rcache, rcname);
    if (kret)
        goto cleanup;
    krb5_rc_recover(kcontext, rcache);

    /* Read our magic number again. */
    kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
    if (kret)
        goto cleanup;
    if (ibuf != KV5M_RCACHE) {
        kret = EINVAL;
        goto cleanup;
    }

    *buffer = bp;
    *lenremain = remain;
    *argp = (krb5_pointer) rcache;
cleanup:
    free(rcname);
    if (kret != 0 && rcache)
        krb5_rc_close(kcontext, rcache);
    return kret;
}

/*
 * Register the rcache serializer.
 */
krb5_error_code KRB5_CALLCONV
krb5_ser_rcache_init(krb5_context kcontext)
{
    return(krb5_register_serializer(kcontext, &krb5_rcache_ser_entry));
}
