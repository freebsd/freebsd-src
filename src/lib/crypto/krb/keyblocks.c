/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/keyblocks.c - Keyblock utility functions */
/*
 * Copyright (C) 2002, 2005 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#include "crypto_int.h"

krb5_error_code
krb5int_c_init_keyblock(krb5_context context, krb5_enctype enctype,
                        size_t length, krb5_keyblock **out)
{
    krb5_keyblock *kb;

    assert(out);
    *out = NULL;

    kb = malloc(sizeof(krb5_keyblock));
    if (kb == NULL)
        return ENOMEM;
    kb->magic = KV5M_KEYBLOCK;
    kb->enctype = enctype;
    kb->length = length;
    if (length) {
        kb->contents = malloc(length);
        if (!kb->contents) {
            free(kb);
            return ENOMEM;
        }
    } else {
        kb->contents = NULL;
    }

    *out = kb;
    return 0;
}

void
krb5int_c_free_keyblock(krb5_context context, register krb5_keyblock *val)
{
    krb5int_c_free_keyblock_contents(context, val);
    free(val);
}

void
krb5int_c_free_keyblock_contents(krb5_context context, krb5_keyblock *key)
{
    if (key && key->contents) {
        zapfree(key->contents, key->length);
        key->contents = NULL;
        key->length = 0;
    }
}

krb5_error_code
krb5int_c_copy_keyblock(krb5_context context, const krb5_keyblock *from,
                        krb5_keyblock **to)
{
    krb5_keyblock *new_key;
    krb5_error_code code;

    *to = NULL;
    new_key = malloc(sizeof(*new_key));
    if (!new_key)
        return ENOMEM;
    code = krb5int_c_copy_keyblock_contents(context, from, new_key);
    if (code) {
        free(new_key);
        return code;
    }
    *to = new_key;
    return 0;
}

krb5_error_code
krb5int_c_copy_keyblock_contents(krb5_context context,
                                 const krb5_keyblock *from, krb5_keyblock *to)
{
    *to = *from;
    if (to->length) {
        to->contents = malloc(to->length);
        if (!to->contents)
            return ENOMEM;
        memcpy(to->contents, from->contents, to->length);
    } else
        to->contents = 0;
    return 0;
}
