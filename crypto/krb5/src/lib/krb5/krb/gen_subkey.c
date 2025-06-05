/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/gen_subkey.c - Generate a subsession key based on input key */
/*
 * Copyright 1991, 2002 by the Massachusetts Institute of Technology.
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

krb5_error_code
krb5_generate_subkey_extended(krb5_context context,
                              const krb5_keyblock *key,
                              krb5_enctype enctype,
                              krb5_keyblock **subkey)
{
    krb5_error_code retval;
    krb5_keyblock *keyblock;

    *subkey = NULL;

    keyblock = malloc(sizeof(krb5_keyblock));
    if (!keyblock)
        return ENOMEM;

    retval = krb5_c_make_random_key(context, enctype, keyblock);
    if (retval) {
        free(*subkey);
        return retval;
    }

    *subkey = keyblock;
    return 0;
}

krb5_error_code
krb5_generate_subkey(krb5_context context, const krb5_keyblock *key, krb5_keyblock **subkey)
{
    return krb5_generate_subkey_extended(context, key, key->enctype, subkey);
}
