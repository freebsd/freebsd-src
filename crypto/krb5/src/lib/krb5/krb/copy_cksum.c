/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/copy_cksum.c */
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
krb5_copy_checksum(krb5_context context, const krb5_checksum *ckfrom, krb5_checksum **ckto)
{
    krb5_checksum *tempto;

    if (!(tempto = (krb5_checksum *)malloc(sizeof(*tempto))))
        return ENOMEM;
    *tempto = *ckfrom;

    if (!(tempto->contents = (krb5_octet *)malloc(tempto->length))) {
        free(tempto);
        return ENOMEM;
    }
    memcpy(tempto->contents, ckfrom->contents, ckfrom->length);

    *ckto = tempto;
    return 0;
}
