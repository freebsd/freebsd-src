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

static const unsigned char kerberos[] = "kerberos";
#define kerberos_len (sizeof(kerberos)-1)

krb5_error_code
krb5int_dk_string_to_key(const struct krb5_keytypes *ktp,
                         const krb5_data *string, const krb5_data *salt,
                         const krb5_data *parms, krb5_keyblock *keyblock)
{
    krb5_error_code ret;
    size_t keybytes, keylength, concatlen;
    unsigned char *concat = NULL, *foldstring = NULL, *foldkeydata = NULL;
    krb5_data indata;
    krb5_keyblock foldkeyblock;
    krb5_key foldkey = NULL;

    /* keyblock->length is checked by krb5int_derive_key. */

    keybytes = ktp->enc->keybytes;
    keylength = ktp->enc->keylength;

    concatlen = string->length + (salt ? salt->length : 0);

    concat = k5alloc(concatlen, &ret);
    if (ret != 0)
        goto cleanup;
    foldstring = k5alloc(keybytes, &ret);
    if (ret != 0)
        goto cleanup;
    foldkeydata = k5alloc(keylength, &ret);
    if (ret != 0)
        goto cleanup;

    /* construct input string ( = string + salt), fold it, make_key it */

    if (string->length > 0)
        memcpy(concat, string->data, string->length);
    if (salt != NULL && salt->length > 0)
        memcpy(concat + string->length, salt->data, salt->length);

    krb5int_nfold(concatlen*8, concat, keybytes*8, foldstring);

    indata.length = keybytes;
    indata.data = (char *) foldstring;
    foldkeyblock.length = keylength;
    foldkeyblock.contents = foldkeydata;
    foldkeyblock.enctype = ktp->etype;

    ret = ktp->rand2key(&indata, &foldkeyblock);
    if (ret != 0)
        goto cleanup;

    ret = krb5_k_create_key(NULL, &foldkeyblock, &foldkey);
    if (ret != 0)
        goto cleanup;

    /* now derive the key from this one */

    indata.length = kerberos_len;
    indata.data = (char *) kerberos;

    ret = krb5int_derive_keyblock(ktp->enc, NULL, foldkey, keyblock, &indata,
                                  DERIVE_RFC3961);
    if (ret != 0)
        memset(keyblock->contents, 0, keyblock->length);

cleanup:
    zapfree(concat, concatlen);
    zapfree(foldstring, keybytes);
    zapfree(foldkeydata, keylength);
    krb5_k_free_key(NULL, foldkey);
    return ret;
}


#define MAX_ITERATION_COUNT             0x1000000L

krb5_boolean k5_allow_weak_pbkdf2iter = FALSE;

static krb5_error_code
pbkdf2_string_to_key(const struct krb5_keytypes *ktp, const krb5_data *string,
                     const krb5_data *salt, const krb5_data *pepper,
                     const krb5_data *params, krb5_keyblock *key,
                     enum deriv_alg deriv_alg, unsigned long def_iter_count)
{
    const struct krb5_hash_provider *hash;
    unsigned long iter_count;
    krb5_data out;
    static const krb5_data usage = { KV5M_DATA, 8, "kerberos" };
    krb5_key tempkey = NULL;
    krb5_error_code err;
    krb5_data sandp = empty_data();

    if (params) {
        unsigned char *p = (unsigned char *) params->data;
        if (params->length != 4)
            return KRB5_ERR_BAD_S2K_PARAMS;
        iter_count = load_32_be(p);
        /* Zero means 2^32, which is way above what we will accept.  Also don't
         * accept values less than the default, unless we're running tests. */
        if (iter_count == 0 ||
            (!k5_allow_weak_pbkdf2iter && iter_count < def_iter_count))
            return KRB5_ERR_BAD_S2K_PARAMS;

    } else
        iter_count = def_iter_count;

    /* This is not a protocol specification constraint; this is an
       implementation limit, which should eventually be controlled by
       a config file.  */
    if (iter_count >= MAX_ITERATION_COUNT)
        return KRB5_ERR_BAD_S2K_PARAMS;

    /* Use the output keyblock contents for temporary space. */
    out.data = (char *) key->contents;
    out.length = key->length;
    if (out.length != 16 && out.length != 32)
        return KRB5_CRYPTO_INTERNAL;

    if (pepper != NULL) {
        err = alloc_data(&sandp, pepper->length + 1 + salt->length);
        if (err)
            return err;

        if (pepper->length > 0)
            memcpy(sandp.data, pepper->data, pepper->length);
        sandp.data[pepper->length] = '\0';
        if (salt->length > 0)
            memcpy(&sandp.data[pepper->length + 1], salt->data, salt->length);

        salt = &sandp;
    }

    hash = (ktp->hash != NULL) ? ktp->hash : &krb5int_hash_sha1;
    err = krb5int_pbkdf2_hmac(hash, &out, iter_count, string, salt);
    if (err)
        goto cleanup;

    err = krb5_k_create_key (NULL, key, &tempkey);
    if (err)
        goto cleanup;

    err = krb5int_derive_keyblock(ktp->enc, ktp->hash, tempkey, key, &usage,
                                  deriv_alg);

cleanup:
    if (sandp.data)
        free(sandp.data);
    if (err)
        memset (out.data, 0, out.length);
    krb5_k_free_key (NULL, tempkey);
    return err;
}

krb5_error_code
krb5int_aes_string_to_key(const struct krb5_keytypes *ktp,
                          const krb5_data *string,
                          const krb5_data *salt,
                          const krb5_data *params,
                          krb5_keyblock *key)
{
    return pbkdf2_string_to_key(ktp, string, salt, NULL, params, key,
                                DERIVE_RFC3961, 4096);
}

krb5_error_code
krb5int_camellia_string_to_key(const struct krb5_keytypes *ktp,
                               const krb5_data *string,
                               const krb5_data *salt,
                               const krb5_data *params,
                               krb5_keyblock *key)
{
    krb5_data pepper = string2data(ktp->name);

    return pbkdf2_string_to_key(ktp, string, salt, &pepper, params, key,
                                DERIVE_SP800_108_CMAC, 32768);
}

krb5_error_code
krb5int_aes2_string_to_key(const struct krb5_keytypes *ktp,
                           const krb5_data *string, const krb5_data *salt,
                           const krb5_data *params, krb5_keyblock *key)
{
    krb5_data pepper = string2data(ktp->name);

    return pbkdf2_string_to_key(ktp, string, salt, &pepper, params, key,
                                DERIVE_SP800_108_HMAC, 32768);
}
