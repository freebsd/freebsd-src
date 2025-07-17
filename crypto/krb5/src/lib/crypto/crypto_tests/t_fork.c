/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_fork.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
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
 * Test basic libk5crypto behavior across forks.  This is primarily interesting
 * for back ends with PKCS11-based constraints.
 */

#include "k5-int.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static krb5_context ctx = NULL;

static void
t(krb5_error_code code)
{
    if (code != 0) {
        fprintf(stderr, "Failure: %s\n", krb5_get_error_message(ctx, code));
        exit(1);
    }
}

static void
prepare_enc_data(krb5_key key, size_t in_len, krb5_enc_data *enc_data)
{
    size_t out_len;

    t(krb5_c_encrypt_length(ctx, key->keyblock.enctype, in_len, &out_len));
    t(alloc_data(&enc_data->ciphertext, out_len));
}

int
main()
{
    krb5_keyblock kb_aes, kb_rc4;
    krb5_key key_aes, key_rc4;
    krb5_data state_rc4, plain = string2data("plain"), decrypted;
    krb5_enc_data out_aes, out_rc4;
    pid_t pid, wpid;
    int status;

    /* Seed the PRNG instead of creating a context, so we don't need
     * krb5.conf. */
    t(krb5_c_random_seed(ctx, &plain));

    /* Create AES and RC4 ciphertexts with random keys.  Use cipher state for
     * RC4. */
    t(krb5_c_make_random_key(ctx, ENCTYPE_AES256_CTS_HMAC_SHA1_96, &kb_aes));
    t(krb5_c_make_random_key(ctx, ENCTYPE_ARCFOUR_HMAC, &kb_rc4));
    t(krb5_k_create_key(ctx, &kb_aes, &key_aes));
    t(krb5_k_create_key(ctx, &kb_rc4, &key_rc4));
    prepare_enc_data(key_aes, plain.length, &out_aes);
    prepare_enc_data(key_aes, plain.length, &out_rc4);
    t(krb5_c_init_state(ctx, &kb_rc4, 0, &state_rc4));
    t(krb5_k_encrypt(ctx, key_aes, 0, NULL, &plain, &out_aes));
    t(krb5_k_encrypt(ctx, key_rc4, 0, &state_rc4, &plain, &out_rc4));

    /* Fork; continue in both parent and child. */
    pid = fork();
    assert(pid >= 0);

    /* Decrypt the AES message with both key and keyblock. */
    t(alloc_data(&decrypted, plain.length));
    t(krb5_k_decrypt(ctx, key_aes, 0, NULL, &out_aes, &decrypted));
    assert(data_eq(plain, decrypted));
    t(krb5_c_decrypt(ctx, &kb_aes, 0, NULL, &out_aes, &decrypted));
    assert(data_eq(plain, decrypted));

    /* Encrypt another RC4 message. */
    t(krb5_k_encrypt(ctx, key_rc4, 0, &state_rc4, &plain, &out_rc4));
    t(krb5_c_free_state(ctx, &kb_rc4, &state_rc4));

    krb5_free_keyblock_contents(ctx, &kb_aes);
    krb5_free_keyblock_contents(ctx, &kb_rc4);
    krb5_k_free_key(ctx, key_aes);
    krb5_k_free_key(ctx, key_rc4);
    krb5_free_data_contents(ctx, &out_aes.ciphertext);
    krb5_free_data_contents(ctx, &out_rc4.ciphertext);
    krb5_free_data_contents(ctx, &decrypted);

    /* If we're the parent, make sure the child succeeded. */
    if (pid != 0) {
        wpid = wait(&status);
        assert(wpid == pid);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child failed with status %d\n", status);
            return 1;
        }
    }

    return 0;
}
