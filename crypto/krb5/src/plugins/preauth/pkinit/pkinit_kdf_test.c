/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/pkinit/pkinit_kdf_test.c */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
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
 * pkinit_kdf_test.c -- Test to verify the correctness of the function
 * pkinit_alg_agility_kdf() in pkinit_crypto_openssl, which implements
 * the Key Derivation Function from the PKInit Algorithm Agility
 * document, currently draft-ietf-krb-wg-pkinit-alg-agility-04.txt.
 */

#include "k5-platform.h"
#include "pkinit.h"

/**
 * Initialize a krb5_data from @a s, a constant string. Note @a s is evaluated
 * multiple times; this is acceptable for constants.
 */
#define DATA_FROM_STRING(s)                     \
    {0, sizeof(s)-1, (char *) s}


/* values from test vectors in the pkinit-alg-agility draft */
int secret_len = 256;
char twenty_as[10];
char eighteen_bs[9];
char party_u_name[] = "lha@SU.SE";
char party_v_name[] = "krbtgt/SU.SE@SU.SE";
int enctype_aes = ENCTYPE_AES256_CTS_HMAC_SHA1_96;
int enctype_des3 = ENCTYPE_DES3_CBC_SHA1;
const krb5_data lha_data = DATA_FROM_STRING("lha");

krb5_octet key1_hex[] =
{0xe6, 0xAB, 0x38, 0xC9, 0x41, 0x3E, 0x03, 0x5B,
 0xB0, 0x79, 0x20, 0x1E, 0xD0, 0xB6, 0xB7, 0x3D,
 0x8D, 0x49, 0xA8, 0x14, 0xA7, 0x37, 0xC0, 0x4E,
 0xE6, 0x64, 0x96, 0x14, 0x20, 0x6F, 0x73, 0xAD};

krb5_octet key2_hex[] =
{0x77, 0xEF, 0x4E, 0x48, 0xC4, 0x20, 0xAE, 0x3F,
 0xEC, 0x75, 0x10, 0x9D, 0x79, 0x81, 0x69, 0x7E,
 0xED, 0x5D, 0x29, 0x5C, 0x90, 0xc6, 0x25, 0x64,
 0xF7, 0xBF, 0xD1, 0x01, 0xFA, 0x9b, 0xC1, 0xD5};

krb5_octet key3_hex[] =
{0xD3, 0xC7, 0x8A, 0x79, 0xD6, 0x52, 0x13, 0xEF,
 0xE9, 0xA8, 0x26, 0xF7, 0x5D, 0xFB, 0x01, 0xF7,
 0x23, 0x62, 0xFB, 0x16, 0xFB, 0x01, 0xDA, 0xD6};

int
main(int argc, char **argv)
{
    /* arguments for calls to pkinit_alg_agility_kdf() */
    krb5_context context = 0;
    krb5_data secret;
    krb5_algorithm_identifier alg_id;
    krb5_data as_req;
    krb5_data pk_as_rep;
    krb5_keyblock key_block;

    /* other local variables */
    int retval = 0;
    krb5_enctype enctype = 0;
    krb5_principal u_principal = NULL;
    krb5_principal v_principal = NULL;

    /* initialize variables that get malloc'ed, so cleanup is safe */
    krb5_init_context (&context);
    memset(&alg_id, 0, sizeof(alg_id));
    memset(&as_req, 0, sizeof(as_req));
    memset(&pk_as_rep, 0, sizeof(pk_as_rep));
    memset(&key_block, 0, sizeof(key_block));

    /* set up a 256-byte, ALL-ZEROS secret */
    if (NULL == (secret.data = malloc(secret_len))) {
        printf("ERROR in pkinit_kdf_test: Memory allocation failed.");
        retval = ENOMEM;
        goto cleanup;
    }
    secret.length = secret_len;
    memset(secret.data, 0, secret_len);

    /* set-up the partyUInfo and partyVInfo principals */
    if ((0 != (retval = krb5_parse_name(context, party_u_name,
                                        &u_principal))) ||
        (0 != (retval = krb5_parse_name(context, party_v_name,
                                        &v_principal)))) {
        printf("ERROR in pkinit_kdf_test: Error parsing names, retval = %d",
               retval);
        goto cleanup;
    }

    /* The test vectors in RFC 8636 implicitly use NT-PRINCIPAL names. */
    u_principal->type = KRB5_NT_PRINCIPAL;
    v_principal->type = KRB5_NT_PRINCIPAL;

    /* set-up the as_req and and pk_as_rep data */
    memset(twenty_as, 0xaa, sizeof(twenty_as));
    memset(eighteen_bs, 0xbb, sizeof(eighteen_bs));
    as_req.length = sizeof(twenty_as);
    as_req.data = twenty_as;

    pk_as_rep.length = sizeof(eighteen_bs);
    pk_as_rep.data = eighteen_bs;

    /* TEST 1:  SHA-1/AES */
    /* set up algorithm id */
    alg_id.algorithm = sha1_id;

    enctype = enctype_aes;

    /* call pkinit_alg_agility_kdf() with test vector values*/
    if (0 != (retval = pkinit_alg_agility_kdf(context, &secret,
                                              &alg_id.algorithm,
                                              u_principal, v_principal,
                                              enctype, &as_req, &pk_as_rep,
                                              &key_block))) {
        printf("ERROR in pkinit_kdf_test: kdf call failed, retval = %d\n",
               retval);
        goto cleanup;
    }

    /* compare key to expected key value */

    if ((key_block.length == sizeof(key1_hex)) &&
        (0 == memcmp(key_block.contents, key1_hex, key_block.length))) {
        printf("SUCCESS: TEST 1 (SHA-1/AES), Correct key value generated.\n");
        retval = 0;
        /* free the keyblock contents, so we can use it for the next test */
        krb5_free_keyblock_contents(context, &key_block);
    } else {
        printf("FAILURE: TEST 1 (SHA-1/AES), Incorrect key value generated!\n");
        retval = 1;
        goto cleanup;
    }

    /* TEST 2: SHA-256/AES */
    /* set up algorithm id */
    alg_id.algorithm = sha256_id;

    enctype = enctype_aes;

    /* call pkinit_alg_agility_kdf() with test vector values*/
    if (0 != (retval = pkinit_alg_agility_kdf(context, &secret,
                                              &alg_id.algorithm,
                                              u_principal, v_principal,
                                              enctype, &as_req, &pk_as_rep,
                                              &key_block))) {
        printf("ERROR in pkinit_kdf_test: kdf call failed, retval = %d\n",
               retval);
        goto cleanup;
    }

    /* compare key to expected key value */

    if ((key_block.length == sizeof(key2_hex)) &&
        (0 == memcmp(key_block.contents, key2_hex, key_block.length))) {
        printf("SUCCESS: TEST 2 (SHA-256/AES), Correct key value generated.\n");
        retval = 0;
        /* free the keyblock contents, so we can use it for the next test */
        krb5_free_keyblock_contents(context, &key_block);
    } else {
        printf("FAILURE: TEST 2 (SHA-256/AES), Incorrect key value generated!\n");
        retval = 1;
        goto cleanup;
    }

    /* TEST 3: SHA-512/DES3 */
    /* set up algorithm id */
    alg_id.algorithm = sha512_id;

    enctype = enctype_des3;

    /* call pkinit_alg_agility_kdf() with test vector values*/
    if (0 != (retval = pkinit_alg_agility_kdf(context, &secret,
                                              &alg_id.algorithm,
                                              u_principal, v_principal,
                                              enctype, &as_req, &pk_as_rep,
                                              &key_block))) {
        printf("ERROR in pkinit_kdf_test: kdf call failed, retval = %d\n",
               retval);
        goto cleanup;
    }

    /* compare key to expected key value */

    if ((key_block.length == sizeof(key3_hex)) &&
        (0 == memcmp(key_block.contents, key3_hex, key_block.length))) {
        printf("SUCCESS: TEST 3 (SHA-512/DES3), Correct key value generated.\n");
        retval = 0;
    } else {
        printf("FAILURE: TEST 2 (SHA-512/DES3), Incorrect key value generated!\n");
        retval = 1;
        goto cleanup;
    }

cleanup:
    /* release all allocated resources, whether good or bad return */
    free(secret.data);
    krb5_free_principal(context, u_principal);
    krb5_free_principal(context, v_principal);
    krb5_free_keyblock_contents(context, &key_block);
    krb5_free_context(context);
    return retval ? 1 : 0;
}
