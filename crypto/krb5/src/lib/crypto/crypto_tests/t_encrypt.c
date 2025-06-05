/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_encrypt.c */
/*
 * Copyright 2001, 2008 by the Massachusetts Institute of Technology.
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

/*
 *
 * <<< Description >>>
 */
/*
 * Some black-box tests of crypto systems.  Make sure that we can decrypt things we encrypt, etc.
 */

#include "crypto_int.h"
#include <stdio.h>

/* What enctypes should we test?*/
krb5_enctype interesting_enctypes[] = {
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

static void
test(const char *msg, krb5_error_code retval)
{
    printf("%s: . . . ", msg);
    if (retval) {
        printf("Failed: %s\n", error_message(retval));
        abort();
    } else
        printf("OK\n");
}

static int compare_results(krb5_data *d1, krb5_data *d2)
{
    if (d1->length != d2->length) {
        /* Decryption can leave a little trailing cruft.
           For the current cryptosystems, this can be up to 7 bytes.  */
        if (d1->length + 8 <= d2->length)
            return EINVAL;
        if (d1->length > d2->length)
            return EINVAL;
    }
    if (memcmp(d1->data, d2->data, d1->length)) {
        return EINVAL;
    }
    return 0;
}

static void
display(const char *msg, const krb5_data *d)
{
    unsigned int i;

    printf("%s:", msg);
    for (i = 0; i < d->length; i++)
        printf(" %02X", (unsigned char) d->data[i]);
    printf("\n");
}

int
main ()
{
    krb5_context context = 0;
    krb5_data  in, in2, out, out2, check, check2, state, signdata;
    krb5_crypto_iov iov[5];
    int i, j, pos;
    unsigned int dummy;
    size_t len;
    krb5_enc_data enc_out, enc_out2;
    krb5_keyblock *keyblock;
    krb5_key key;

    memset(iov, 0, sizeof(iov));

    in.data = "This is a test.\n";
    in.length = strlen (in.data);
    in2.data = "This is another test.\n";
    in2.length = strlen (in2.data);

    test ("Seeding random number generator",
          krb5_c_random_seed (context, &in));

    /* Set up output buffers. */
    out.data = malloc(2048);
    out2.data = malloc(2048);
    check.data = malloc(2048);
    check2.data = malloc(2048);
    if (out.data == NULL || out2.data == NULL
        || check.data == NULL || check2.data == NULL)
        abort();
    out.magic = KV5M_DATA;
    out.length = 2048;
    out2.magic = KV5M_DATA;
    out2.length = 2048;
    check.length = 2048;
    check2.length = 2048;

    for (i = 0; interesting_enctypes[i]; i++) {
        krb5_enctype enctype = interesting_enctypes [i];

        printf ("Testing enctype %d\n", enctype);
        test ("Initializing a keyblock",
              krb5_init_keyblock (context, enctype, 0, &keyblock));
        test ("Generating random keyblock",
              krb5_c_make_random_key (context, enctype, keyblock));
        test ("Creating opaque key from keyblock",
              krb5_k_create_key (context, keyblock, &key));

        enc_out.ciphertext = out;
        enc_out2.ciphertext = out2;
        /* We use an intermediate `len' because size_t may be different size
           than `int' */
        krb5_c_encrypt_length (context, keyblock->enctype, in.length, &len);
        enc_out.ciphertext.length = len;

        /* Encrypt, decrypt, and see if we got the plaintext back again. */
        test ("Encrypting (c)",
              krb5_c_encrypt (context, keyblock, 7, 0, &in, &enc_out));
        display ("Enc output", &enc_out.ciphertext);
        test ("Decrypting",
              krb5_c_decrypt (context, keyblock, 7, 0, &enc_out, &check));
        test ("Comparing", compare_results (&in, &check));

        /* Try again with the opaque-key-using variants. */
        memset(out.data, 0, out.length);
        test ("Encrypting (k)",
              krb5_k_encrypt (context, key, 7, 0, &in, &enc_out));
        display ("Enc output", &enc_out.ciphertext);
        test ("Decrypting",
              krb5_k_decrypt (context, key, 7, 0, &enc_out, &check));
        test ("Comparing", compare_results (&in, &check));

        /* Check if this enctype supports IOV encryption. */
        if ( krb5_c_crypto_length(context, keyblock->enctype,
                                  KRB5_CRYPTO_TYPE_HEADER, &dummy) == 0 ){
            /* Set up iovecs for stream decryption. */
            memcpy(out2.data, enc_out.ciphertext.data, enc_out.ciphertext.length);
            iov[0].flags= KRB5_CRYPTO_TYPE_STREAM;
            iov[0].data.data = out2.data;
            iov[0].data.length = enc_out.ciphertext.length;
            iov[1].flags = KRB5_CRYPTO_TYPE_DATA;

            /* Decrypt the encrypted data from above and check it. */
            test("IOV stream decrypting (c)",
                 krb5_c_decrypt_iov( context, keyblock, 7, 0, iov, 2));
            test("Comparing results",
                 compare_results(&in, &iov[1].data));

            /* Try again with the opaque-key-using variant. */
            memcpy(out2.data, enc_out.ciphertext.data, enc_out.ciphertext.length);
            test("IOV stream decrypting (k)",
                 krb5_k_decrypt_iov( context, key, 7, 0, iov, 2));
            test("Comparing results",
                 compare_results(&in, &iov[1].data));

            /* Set up iovecs for AEAD encryption. */
            signdata.magic = KV5M_DATA;
            signdata.data = (char *) "This should be signed";
            signdata.length = strlen(signdata.data);
            iov[0].flags = KRB5_CRYPTO_TYPE_HEADER;
            iov[1].flags = KRB5_CRYPTO_TYPE_DATA;
            iov[1].data = in; /*We'll need to copy memory before encrypt*/
            iov[2].flags = KRB5_CRYPTO_TYPE_SIGN_ONLY;
            iov[2].data = signdata;
            iov[3].flags = KRB5_CRYPTO_TYPE_PADDING;
            iov[4].flags = KRB5_CRYPTO_TYPE_TRAILER;

            /* "Allocate" data for the iovec buffers from the "out" buffer. */
            test("Setting up iov lengths",
                 krb5_c_crypto_length_iov(context, keyblock->enctype, iov, 5));
            for (j=0,pos=0; j <= 4; j++ ){
                if (iov[j].flags == KRB5_CRYPTO_TYPE_SIGN_ONLY)
                    continue;
                iov[j].data.data = &out.data[pos];
                pos += iov[j].data.length;
            }
            assert (iov[1].data.length == in.length);
            memcpy(iov[1].data.data, in.data, in.length);

            /* Encrypt and decrypt in place, and check the result. */
            test("iov encrypting (c)",
                 krb5_c_encrypt_iov(context, keyblock, 7, 0, iov, 5));
            assert(iov[1].data.length == in.length);
            display("Header", &iov[0].data);
            display("Data", &iov[1].data);
            display("Padding", &iov[3].data);
            display("Trailer", &iov[4].data);
            test("iov decrypting",
                 krb5_c_decrypt_iov(context, keyblock, 7, 0, iov, 5));
            test("Comparing results",
                 compare_results(&in, &iov[1].data));

            /* Try again with opaque-key-using variants. */
            test("iov encrypting (k)",
                 krb5_k_encrypt_iov(context, key, 7, 0, iov, 5));
            assert(iov[1].data.length == in.length);
            display("Header", &iov[0].data);
            display("Data", &iov[1].data);
            display("Padding", &iov[3].data);
            display("Trailer", &iov[4].data);
            test("iov decrypting",
                 krb5_k_decrypt_iov(context, key, 7, 0, iov, 5));
            test("Comparing results",
                 compare_results(&in, &iov[1].data));
        }

        enc_out.ciphertext.length = out.length;
        check.length = 2048;

        test ("init_state",
              krb5_c_init_state (context, keyblock, 7, &state));
        test ("Encrypting with state",
              krb5_c_encrypt (context, keyblock, 7, &state, &in, &enc_out));
        display ("Enc output", &enc_out.ciphertext);
        test ("Encrypting again with state",
              krb5_c_encrypt (context, keyblock, 7, &state, &in2, &enc_out2));
        display ("Enc output", &enc_out2.ciphertext);
        test ("free_state",
              krb5_c_free_state (context, keyblock, &state));
        test ("init_state",
              krb5_c_init_state (context, keyblock, 7, &state));
        test ("Decrypting with state",
              krb5_c_decrypt (context, keyblock, 7, &state, &enc_out, &check));
        test ("Decrypting again with state",
              krb5_c_decrypt (context, keyblock, 7, &state, &enc_out2, &check2));
        test ("free_state",
              krb5_c_free_state (context, keyblock, &state));
        test ("Comparing",
              compare_results (&in, &check));
        test ("Comparing",
              compare_results (&in2, &check2));

        krb5_free_keyblock (context, keyblock);
        krb5_k_free_key (context, key);
    }

    /* Test the RC4 decrypt fallback from key usage 9 to 8. */
    test ("Initializing an RC4 keyblock",
          krb5_init_keyblock (context, ENCTYPE_ARCFOUR_HMAC, 0, &keyblock));
    test ("Generating random RC4 key",
          krb5_c_make_random_key (context, ENCTYPE_ARCFOUR_HMAC, keyblock));
    enc_out.ciphertext = out;
    krb5_c_encrypt_length (context, keyblock->enctype, in.length, &len);
    enc_out.ciphertext.length = len;
    check.length = 2048;
    test ("Encrypting with RC4 key usage 8",
          krb5_c_encrypt (context, keyblock, 8, 0, &in, &enc_out));
    display ("Enc output", &enc_out.ciphertext);
    test ("Decrypting with RC4 key usage 9",
          krb5_c_decrypt (context, keyblock, 9, 0, &enc_out, &check));
    test ("Comparing", compare_results (&in, &check));

    krb5_free_keyblock (context, keyblock);
    free(out.data);
    free(out2.data);
    free(check.data);
    free(check2.data);
    return 0;
}
