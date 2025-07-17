/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
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
 *
 * Functions for manipulating krb5_key structures
 */

#include "crypto_int.h"

/*
 * The krb5_key data type wraps an exposed keyblock in an opaque data
 * structure, to allow for internal optimizations such as caching of
 * derived keys.
 */

/* Create a krb5_key from the enctype and key data in a keyblock. */
krb5_error_code KRB5_CALLCONV
krb5_k_create_key(krb5_context context, const krb5_keyblock *key_data,
                  krb5_key *out)
{
    krb5_key key = NULL;
    krb5_error_code code;

    *out = NULL;

    key = malloc(sizeof(*key));
    if (key == NULL)
        return ENOMEM;
    code = krb5int_c_copy_keyblock_contents(context, key_data, &key->keyblock);
    if (code)
        goto cleanup;

    key->refcount = 1;
    key->derived = NULL;
    key->cache = NULL;
    *out = key;
    return 0;

cleanup:
    free(key);
    return code;
}

void KRB5_CALLCONV
krb5_k_reference_key(krb5_context context, krb5_key key)
{
    if (key)
        key->refcount++;
}

/* Free the memory used by a krb5_key. */
void KRB5_CALLCONV
krb5_k_free_key(krb5_context context, krb5_key key)
{
    struct derived_key *dk;
    const struct krb5_keytypes *ktp;

    if (key == NULL || --key->refcount > 0)
        return;

    /* Free the derived key cache. */
    while ((dk = key->derived) != NULL) {
        key->derived = dk->next;
        free(dk->constant.data);
        krb5_k_free_key(context, dk->dkey);
        free(dk);
    }
    krb5int_c_free_keyblock_contents(context, &key->keyblock);
    if (key->cache) {
        ktp = find_enctype(key->keyblock.enctype);
        if (ktp && ktp->enc->key_cleanup)
            ktp->enc->key_cleanup(key);
    }
    free(key);
}

/* Retrieve a copy of the keyblock from a krb5_key. */
krb5_error_code KRB5_CALLCONV
krb5_k_key_keyblock(krb5_context context, krb5_key key,
                    krb5_keyblock **key_data)
{
    return krb5int_c_copy_keyblock(context, &key->keyblock, key_data);
}

/* Retrieve the enctype of a krb5_key. */
krb5_enctype KRB5_CALLCONV
krb5_k_key_enctype(krb5_context context, krb5_key key)
{
    return key->keyblock.enctype;
}
