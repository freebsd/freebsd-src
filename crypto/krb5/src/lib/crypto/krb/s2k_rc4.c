/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "crypto_int.h"
#include "k5-utf8.h"

krb5_error_code
krb5int_arcfour_string_to_key(const struct krb5_keytypes *ktp,
                              const krb5_data *string, const krb5_data *salt,
                              const krb5_data *params, krb5_keyblock *key)
{
    krb5_error_code err = 0;
    krb5_crypto_iov iov;
    krb5_data hash_out;
    char *utf8;
    unsigned char *copystr;
    size_t copystrlen;

    if (params != NULL)
        return KRB5_ERR_BAD_S2K_PARAMS;

    if (key->length != 16)
        return (KRB5_BAD_MSIZE);

    /* We ignore salt per the Microsoft spec. */
    utf8 = k5memdup0(string->data, string->length, &err);
    if (utf8 == NULL)
        return err;
    err = k5_utf8_to_utf16le(utf8, &copystr, &copystrlen);
    zapfree(utf8, string->length);
    if (err)
        return err;

    /* the actual MD4 hash of the data */
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = make_data(copystr, copystrlen);
    hash_out = make_data(key->contents, key->length);
    err = krb5int_hash_md4.hash(&iov, 1, &hash_out);

    /* Zero out the data behind us */
    zapfree(copystr, copystrlen);
    return err;
}
