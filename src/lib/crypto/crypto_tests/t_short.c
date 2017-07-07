/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_short.c */
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
 */

/*
 * Tests the outcome of decrypting overly short tokens.  This program can be
 * run under a tool like valgrind to detect bad memory accesses; when run
 * normally by the test suite, it verifies that each operation returns
 * KRB5_BAD_MSIZE.
 */

#include "k5-int.h"

krb5_enctype interesting_enctypes[] = {
    ENCTYPE_DES_CBC_CRC,
    ENCTYPE_DES_CBC_MD4,
    ENCTYPE_DES_CBC_MD5,
    ENCTYPE_DES3_CBC_SHA1,
    ENCTYPE_ARCFOUR_HMAC,
    ENCTYPE_ARCFOUR_HMAC_EXP,
    ENCTYPE_AES256_CTS_HMAC_SHA1_96,
    ENCTYPE_AES128_CTS_HMAC_SHA1_96,
    ENCTYPE_CAMELLIA128_CTS_CMAC,
    ENCTYPE_CAMELLIA256_CTS_CMAC,
    ENCTYPE_AES128_CTS_HMAC_SHA256_128,
    ENCTYPE_AES256_CTS_HMAC_SHA384_192,
    0
};

/* Abort if an operation unexpectedly fails. */
static void
x(krb5_error_code code)
{
    if (code != 0)
        abort();
}

/* Abort if a decrypt operation doesn't have the expected result. */
static void
check_decrypt_result(krb5_error_code code, size_t len, size_t min_len)
{
    if (len < min_len) {
        /* Undersized tokens should always result in BAD_MSIZE. */
        if (code != KRB5_BAD_MSIZE)
            abort();
    } else {
        /* Min-size tokens should succeed or fail the integrity check. */
        if (code != 0 && code != KRB5KRB_AP_ERR_BAD_INTEGRITY)
            abort();
    }
}

static void
test_enctype(krb5_enctype enctype)
{
    krb5_error_code ret;
    krb5_keyblock keyblock;
    krb5_enc_data input;
    krb5_data output;
    krb5_crypto_iov iov[2];
    unsigned int dummy;
    size_t min_len, len;

    printf("Testing enctype %d\n", (int) enctype);
    x(krb5_c_encrypt_length(NULL, enctype, 0, &min_len));
    x(krb5_c_make_random_key(NULL, enctype, &keyblock));
    input.enctype = enctype;

    /* Try each length up to the minimum length. */
    for (len = 0; len <= min_len; len++) {
        input.ciphertext.data = calloc(len, 1);
        input.ciphertext.length = len;
        output.data = calloc(len, 1);
        output.length = len;

        /* Attempt a normal decryption. */
        ret = krb5_c_decrypt(NULL, &keyblock, 0, NULL, &input, &output);
        check_decrypt_result(ret, len, min_len);

        if (krb5_c_crypto_length(NULL, enctype, KRB5_CRYPTO_TYPE_HEADER,
                                 &dummy) == 0) {
            /* Attempt an IOV stream decryption. */
            iov[0].flags = KRB5_CRYPTO_TYPE_STREAM;
            iov[0].data = input.ciphertext;
            iov[1].flags = KRB5_CRYPTO_TYPE_DATA;
            iov[1].data.data = NULL;
            iov[1].data.length = 0;
            ret = krb5_c_decrypt_iov(NULL, &keyblock, 0, NULL, iov, 2);
            check_decrypt_result(ret, len, min_len);
        }

        free(input.ciphertext.data);
        free(output.data);
    }
    krb5int_c_free_keyblock_contents (NULL, &keyblock);

}

int
main(int argc, char **argv)
{
    int i;
    krb5_data notrandom;

    notrandom.data = "notrandom";
    notrandom.length = 9;
    krb5_c_random_seed(NULL, &notrandom);
    for (i = 0; interesting_enctypes[i]; i++)
        test_enctype(interesting_enctypes[i]);
    return 0;
}
