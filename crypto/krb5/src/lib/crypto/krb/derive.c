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

static krb5_key
find_cached_dkey(struct derived_key *list, const krb5_data *constant)
{
    for (; list; list = list->next) {
        if (data_eq(list->constant, *constant)) {
            krb5_k_reference_key(NULL, list->dkey);
            return list->dkey;
        }
    }
    return NULL;
}

static krb5_error_code
add_cached_dkey(krb5_key key, const krb5_data *constant,
                const krb5_keyblock *dkeyblock, krb5_key *cached_dkey)
{
    krb5_key dkey;
    krb5_error_code ret;
    struct derived_key *dkent = NULL;
    char *data = NULL;

    /* Allocate fields for the new entry. */
    dkent = malloc(sizeof(*dkent));
    if (dkent == NULL)
        goto cleanup;
    data = k5memdup(constant->data, constant->length, &ret);
    if (data == NULL)
        goto cleanup;
    ret = krb5_k_create_key(NULL, dkeyblock, &dkey);
    if (ret != 0)
        goto cleanup;

    /* Add the new entry to the list. */
    dkent->dkey = dkey;
    dkent->constant.data = data;
    dkent->constant.length = constant->length;
    dkent->next = key->derived;
    key->derived = dkent;

    /* Return a "copy" of the cached key. */
    krb5_k_reference_key(NULL, dkey);
    *cached_dkey = dkey;
    return 0;

cleanup:
    free(dkent);
    free(data);
    return ENOMEM;
}

krb5_error_code
krb5int_derive_random(const struct krb5_enc_provider *enc,
                      const struct krb5_hash_provider *hash,
                      krb5_key inkey, krb5_data *outrnd,
                      const krb5_data *in_constant, enum deriv_alg alg)
{
    krb5_data empty = empty_data();

    switch (alg) {
    case DERIVE_RFC3961:
        return k5_derive_random_rfc3961(enc, inkey, in_constant, outrnd);
    case DERIVE_SP800_108_CMAC:
        return k5_sp800_108_feedback_cmac(enc, inkey, in_constant, outrnd);
    case DERIVE_SP800_108_HMAC:
        return k5_sp800_108_counter_hmac(hash, inkey, in_constant, &empty,
                                         outrnd);
    default:
        return EINVAL;
    }
}

/*
 * Compute a derived key into the keyblock outkey.  This variation on
 * krb5int_derive_key does not cache the result, as it is only used
 * directly in situations which are not expected to be repeated with
 * the same inkey and constant.
 */
krb5_error_code
krb5int_derive_keyblock(const struct krb5_enc_provider *enc,
                        const struct krb5_hash_provider *hash,
                        krb5_key inkey, krb5_keyblock *outkey,
                        const krb5_data *in_constant, enum deriv_alg alg)
{
    krb5_error_code ret;
    krb5_data rawkey = empty_data();

    /* Allocate a buffer for the raw key bytes. */
    ret = alloc_data(&rawkey, enc->keybytes);
    if (ret)
        goto cleanup;

    /* Derive pseudo-random data for the key bytes. */
    ret = krb5int_derive_random(enc, hash, inkey, &rawkey, in_constant, alg);
    if (ret)
        goto cleanup;

    /* Postprocess the key. */
    ret = krb5_c_random_to_key(NULL, inkey->keyblock.enctype, &rawkey, outkey);

cleanup:
    zapfree(rawkey.data, enc->keybytes);
    return ret;
}

krb5_error_code
krb5int_derive_key(const struct krb5_enc_provider *enc,
                   const struct krb5_hash_provider *hash,
                   krb5_key inkey, krb5_key *outkey,
                   const krb5_data *in_constant, enum deriv_alg alg)
{
    krb5_keyblock keyblock;
    krb5_error_code ret;
    krb5_key dkey;

    *outkey = NULL;

    /* Check for a cached result. */
    dkey = find_cached_dkey(inkey->derived, in_constant);
    if (dkey != NULL) {
        *outkey = dkey;
        return 0;
    }

    /* Derive into a temporary keyblock. */
    keyblock.length = enc->keylength;
    keyblock.contents = malloc(keyblock.length);
    keyblock.enctype = inkey->keyblock.enctype;
    if (keyblock.contents == NULL)
        return ENOMEM;
    ret = krb5int_derive_keyblock(enc, hash, inkey, &keyblock, in_constant,
                                  alg);
    if (ret)
        goto cleanup;

    /* Cache the derived key. */
    ret = add_cached_dkey(inkey, in_constant, &keyblock, &dkey);
    if (ret != 0)
        goto cleanup;

    *outkey = dkey;

cleanup:
    zapfree(keyblock.contents, keyblock.length);
    return ret;
}
