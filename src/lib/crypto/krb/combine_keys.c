/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* Copyright (c) 2002 Naval Research Laboratory (NRL/CCS) */
/*
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the software,
 * derivative works or modified versions, and any portions thereof.
 *
 * NRL ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION AND
 * DISCLAIMS ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
 * RESULTING FROM THE USE OF THIS SOFTWARE.
 */

/*
 * Key combination function.
 *
 * If Key1 and Key2 are two keys to be combined, the algorithm to combine
 * them is as follows.
 *
 * Definitions:
 *
 * k-truncate is defined as truncating to the key size the input.
 *
 * DR is defined as the generate "random" data from a key
 * (defined in crypto draft)
 *
 * DK is defined as the key derivation function (krb5int_derive_key())
 *
 * (note: | means "concatenate")
 *
 * Combine key algorithm:
 *
 * R1 = DR(Key1, n-fold(Key2)) [ Output is length of Key1 ]
 * R2 = DR(Key2, n-fold(Key1)) [ Output is length of Key2 ]
 *
 * rnd = n-fold(R1 | R2) [ Note: output size of nfold must be appropriately
 *                         sized for random-to-key function ]
 * tkey = random-to-key(rnd)
 * Combine-Key(Key1, Key2) = DK(tkey, CombineConstant)
 *
 * CombineConstant is defined as the byte string:
 *
 * { 0x63 0x6f 0x6d 0x62 0x69 0x6e 0x65 }, which corresponds to the
 * ASCII encoding of the string "combine"
 */

#include "crypto_int.h"

static krb5_error_code dr(const struct krb5_enc_provider *enc,
                          const krb5_keyblock *inkey, unsigned char *outdata,
                          const krb5_data *in_constant);

/*
 * We only support this combine_keys algorithm for des and 3des keys.
 * Everything else should use the PRF defined in the crypto framework.
 * We don't implement that yet.
 */

static krb5_boolean
enctype_ok(krb5_enctype e)
{
    switch (e) {
    case ENCTYPE_DES_CBC_CRC:
    case ENCTYPE_DES_CBC_MD4:
    case ENCTYPE_DES_CBC_MD5:
    case ENCTYPE_DES3_CBC_SHA1:
        return TRUE;
    default:
        return FALSE;
    }
}

krb5_error_code
krb5int_c_combine_keys(krb5_context context, krb5_keyblock *key1,
                       krb5_keyblock *key2, krb5_keyblock *outkey)
{
    unsigned char *r1 = NULL, *r2 = NULL, *combined = NULL, *rnd = NULL;
    unsigned char *output = NULL;
    size_t keybytes, keylength;
    const struct krb5_enc_provider *enc;
    krb5_data input, randbits;
    krb5_keyblock tkeyblock;
    krb5_key tkey = NULL;
    krb5_error_code ret;
    const struct krb5_keytypes *ktp;
    krb5_boolean myalloc = FALSE;

    if (!enctype_ok(key1->enctype) || !enctype_ok(key2->enctype))
        return KRB5_CRYPTO_INTERNAL;

    if (key1->length != key2->length || key1->enctype != key2->enctype)
        return KRB5_CRYPTO_INTERNAL;

    /* Find our encryption algorithm. */
    ktp = find_enctype(key1->enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;
    enc = ktp->enc;

    keybytes = enc->keybytes;
    keylength = enc->keylength;

    /* Allocate and set up buffers. */
    r1 = k5alloc(keybytes, &ret);
    if (ret)
        goto cleanup;
    r2 = k5alloc(keybytes, &ret);
    if (ret)
        goto cleanup;
    rnd = k5alloc(keybytes, &ret);
    if (ret)
        goto cleanup;
    combined = k5calloc(2, keybytes, &ret);
    if (ret)
        goto cleanup;
    output = k5alloc(keylength, &ret);
    if (ret)
        goto cleanup;

    /*
     * Get R1 and R2 (by running the input keys through the DR algorithm.
     * Note this is most of derive-key, but not all.
     */

    input.length = key2->length;
    input.data = (char *) key2->contents;
    ret = dr(enc, key1, r1, &input);
    if (ret)
        goto cleanup;

    input.length = key1->length;
    input.data = (char *) key1->contents;
    ret = dr(enc, key2, r2, &input);
    if (ret)
        goto cleanup;

    /*
     * Concatenate the two keys together, and then run them through
     * n-fold to reduce them to a length appropriate for the random-to-key
     * operation.  Note here that krb5int_nfold() takes sizes in bits, hence
     * the multiply by 8.
     */

    memcpy(combined, r1, keybytes);
    memcpy(combined + keybytes, r2, keybytes);

    krb5int_nfold((keybytes * 2) * 8, combined, keybytes * 8, rnd);

    /*
     * Run the "random" bits through random-to-key to produce a encryption
     * key.
     */

    randbits.length = keybytes;
    randbits.data = (char *) rnd;
    tkeyblock.length = keylength;
    tkeyblock.contents = output;
    tkeyblock.enctype = key1->enctype;

    ret = (*ktp->rand2key)(&randbits, &tkeyblock);
    if (ret)
        goto cleanup;

    ret = krb5_k_create_key(NULL, &tkeyblock, &tkey);
    if (ret)
        goto cleanup;

    /*
     * Run through derive-key one more time to produce the final key.
     * Note that the input to derive-key is the ASCII string "combine".
     */

    input.length = 7;
    input.data = "combine";

    /*
     * Just FYI: _if_ we have space here in the key, then simply use it
     * without modification.  But if the key is blank (no allocated storage)
     * then allocate some memory for it.  This allows programs to use one of
     * the existing keys as the output key, _or_ pass in a blank keyblock
     * for us to allocate.  It's easier for us to allocate it since we already
     * know the crypto library internals
     */

    if (outkey->length == 0 || outkey->contents == NULL) {
        outkey->contents = k5alloc(keylength, &ret);
        if (ret)
            goto cleanup;
        outkey->length = keylength;
        outkey->enctype = key1->enctype;
        myalloc = TRUE;
    }

    ret = krb5int_derive_keyblock(enc, NULL, tkey, outkey, &input,
                                  DERIVE_RFC3961);
    if (ret) {
        if (myalloc) {
            free(outkey->contents);
            outkey->contents = NULL;
        }
        goto cleanup;
    }

cleanup:
    zapfree(r1, keybytes);
    zapfree(r2, keybytes);
    zapfree(rnd, keybytes);
    zapfree(combined, keybytes * 2);
    zapfree(output, keylength);
    krb5_k_free_key(NULL, tkey);
    return ret;
}

/* Our DR function, a simple wrapper around krb5int_derive_random(). */
static krb5_error_code
dr(const struct krb5_enc_provider *enc, const krb5_keyblock *inkey,
   unsigned char *out, const krb5_data *in_constant)
{
    krb5_data outdata = make_data(out, enc->keybytes);
    krb5_key key = NULL;
    krb5_error_code ret;

    ret = krb5_k_create_key(NULL, inkey, &key);
    if (ret != 0)
        return ret;
    ret = krb5int_derive_random(enc, NULL, key, &outdata, in_constant,
                                DERIVE_RFC3961);
    krb5_k_free_key(NULL, key);
    return ret;
}
