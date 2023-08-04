/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/pr_to_salt.c */
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

static krb5_error_code
principal2salt_internal(krb5_context, krb5_const_principal,
                        krb5_data *ret, int);

/*
 * Convert a krb5_principal into the default salt for that principal.
 */
static krb5_error_code
principal2salt_internal(krb5_context context, krb5_const_principal pr,
                        krb5_data *ret, int use_realm)
{
    unsigned int size = 0, offset=0;
    krb5_int32 i;

    *ret = empty_data();
    if (pr == NULL)
        return 0;

    if (use_realm)
        size += pr->realm.length;

    for (i = 0; i < pr->length; i++)
        size += pr->data[i].length;

    if (alloc_data(ret, size))
        return ENOMEM;

    if (use_realm) {
        offset = pr->realm.length;
        if (offset > 0)
            memcpy(ret->data, pr->realm.data, offset);
    }

    for (i = 0; i < pr->length; i++) {
        if (pr->data[i].length > 0)
            memcpy(&ret->data[offset], pr->data[i].data, pr->data[i].length);
        offset += pr->data[i].length;
    }
    return 0;
}

krb5_error_code
krb5_principal2salt(krb5_context context, krb5_const_principal pr,
                    krb5_data *ret)
{
    return principal2salt_internal(context, pr, ret, 1);
}

krb5_error_code
krb5_principal2salt_norealm(krb5_context context, krb5_const_principal pr,
                            krb5_data *ret)
{
    return principal2salt_internal(context, pr, ret, 0);
}
