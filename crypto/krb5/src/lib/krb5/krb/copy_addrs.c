/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/copy_addrs.c */
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

#include "k5-int.h"

krb5_error_code KRB5_CALLCONV
krb5_copy_addr(krb5_context context, const krb5_address *inad, krb5_address **outad)
{
    krb5_address *tmpad;

    if (!(tmpad = (krb5_address *)malloc(sizeof(*tmpad))))
        return ENOMEM;
    *tmpad = *inad;
    if (!(tmpad->contents = (krb5_octet *)malloc(inad->length))) {
        free(tmpad);
        return ENOMEM;
    }
    memcpy(tmpad->contents, inad->contents, inad->length);
    *outad = tmpad;
    return 0;
}

/*
 * Copy an address array, with fresh allocation.
 */
krb5_error_code KRB5_CALLCONV
krb5_copy_addresses(krb5_context context, krb5_address *const *inaddr, krb5_address ***outaddr)
{
    krb5_error_code retval;
    krb5_address ** tempaddr;
    unsigned int nelems = 0;

    if (!inaddr) {
        *outaddr = 0;
        return 0;
    }

    while (inaddr[nelems]) nelems++;

    /* one more for a null terminated list */
    if (!(tempaddr = (krb5_address **) calloc(nelems+1, sizeof(*tempaddr))))
        return ENOMEM;

    for (nelems = 0; inaddr[nelems]; nelems++) {
        retval = krb5_copy_addr(context, inaddr[nelems], &tempaddr[nelems]);
        if (retval) {
            krb5_free_addresses(context, tempaddr);
            return retval;
        }
    }

    *outaddr = tempaddr;
    return 0;
}
