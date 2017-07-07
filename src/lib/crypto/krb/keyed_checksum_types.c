/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "crypto_int.h"

static krb5_boolean
is_keyed_for(const struct krb5_cksumtypes *ctp,
             const struct krb5_keytypes *ktp)
{
    if (ctp->flags & CKSUM_UNKEYED)
        return FALSE;
    return (!ctp->enc || ktp->enc == ctp->enc);
}

krb5_error_code KRB5_CALLCONV
krb5_c_keyed_checksum_types(krb5_context context, krb5_enctype enctype,
                            unsigned int *count, krb5_cksumtype **cksumtypes)
{
    unsigned int i, c, nctypes;
    krb5_cksumtype *ctypes;
    const struct krb5_cksumtypes *ctp;
    const struct krb5_keytypes *ktp;

    *count = 0;
    *cksumtypes = NULL;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    nctypes = 0;
    for (i = 0; i < krb5int_cksumtypes_length; i++) {
        ctp = &krb5int_cksumtypes_list[i];
        if (is_keyed_for(ctp, ktp))
            nctypes++;
    }

    ctypes = malloc(nctypes * sizeof(krb5_cksumtype));
    if (ctypes == NULL)
        return ENOMEM;

    c = 0;
    for (i = 0; i < krb5int_cksumtypes_length; i++) {
        ctp = &krb5int_cksumtypes_list[i];
        if (is_keyed_for(ctp, ktp))
            ctypes[c++] = ctp->ctype;
    }

    *count = nctypes;
    *cksumtypes = ctypes;
    return 0;
}

void KRB5_CALLCONV
krb5_free_cksumtypes(krb5_context context, krb5_cksumtype *val)
{
    free(val);
}
