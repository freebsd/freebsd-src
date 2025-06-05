/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_cmac.c */
/*
 * Copyright 2010 by the Massachusetts Institute of Technology.
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
 * Test vectors for CMAC.  Inputs are taken from RFC 4493 section 4.  Outputs
 * are changed for the use of Camellia-128 in place of AES-128.
 *
 * Ideally we would double-check subkey values, but we have no easy way to see
 * them.
 *
 * Ideally we would test AES-CMAC against the expected results in RFC 4493,
 * instead of Camellia-CMAC against results we generated ourselves.  This has
 * been done manually, but is not convenient to do automatically since the
 * AES-128 enc provider has no cbc_mac method and therefore cannot be used with
 * krb5int_cmac_checksum.
 */

#include "crypto_int.h"

/* All examples use the following Camellia-128 key. */
static unsigned char keybytes[] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

/* Example inputs are this message truncated to 0, 16, 40, and 64 bytes. */
unsigned char input[] = {
    0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
    0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
    0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
    0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
    0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
    0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
    0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
};

/* Expected result of CMAC on empty input. */
static unsigned char cmac1[] = {
    0xba, 0x92, 0x57, 0x82, 0xaa, 0xa1, 0xf5, 0xd9,
    0xa0, 0x0f, 0x89, 0x64, 0x80, 0x94, 0xfc, 0x71
};

/* Expected result of CMAC on first 16 bytes of input. */
static unsigned char cmac2[] = {
    0x6d, 0x96, 0x28, 0x54, 0xa3, 0xb9, 0xfd, 0xa5,
    0x6d, 0x7d, 0x45, 0xa9, 0x5e, 0xe1, 0x79, 0x93
};

/* Expected result of CMAC on first 40 bytes of input. */
static unsigned char cmac3[] = {
    0x5c, 0x18, 0xd1, 0x19, 0xcc, 0xd6, 0x76, 0x61,
    0x44, 0xac, 0x18, 0x66, 0x13, 0x1d, 0x9f, 0x22
};

/* Expected result of CMAC on all 64 bytes of input. */
static unsigned char cmac4[] = {
    0xc2, 0x69, 0x9a, 0x6e, 0xba, 0x55, 0xce, 0x9d,
    0x93, 0x9a, 0x8a, 0x4e, 0x19, 0x46, 0x6e, 0xe9
};

static void
check_result(const char *name, const unsigned char *result,
             const unsigned char *expected)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (result[i] != expected[i]) {
            fprintf(stderr, "CMAC test vector failure: %s\n", name);
            exit(1);
        }
    }
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context = NULL;
    krb5_keyblock keyblock;
    krb5_key key;
    const struct krb5_enc_provider *enc = &krb5int_enc_camellia128;
    krb5_crypto_iov iov;
    unsigned char resultbuf[16];
    krb5_data result = make_data(resultbuf, 16);

    /* Create the example key. */
    keyblock.magic = KV5M_KEYBLOCK;
    keyblock.enctype = ENCTYPE_CAMELLIA128_CTS_CMAC;
    keyblock.length = 16;
    keyblock.contents = keybytes;
    ret = krb5_k_create_key(context, &keyblock, &key);
    assert(!ret);

    /* Example 1. */
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = make_data(input, 0);
    ret = krb5int_cmac_checksum(enc, key, &iov, 1, &result);
    assert(!ret);
    check_result("example 1", resultbuf, cmac1);

    /* Example 2. */
    iov.data.length = 16;
    ret = krb5int_cmac_checksum(enc, key, &iov, 1, &result);
    assert(!ret);
    check_result("example 2", resultbuf, cmac2);

    /* Example 3. */
    iov.data.length = 40;
    ret = krb5int_cmac_checksum(enc, key, &iov, 1, &result);
    assert(!ret);
    check_result("example 3", resultbuf, cmac3);

    /* Example 4. */
    iov.data.length = 64;
    ret = krb5int_cmac_checksum(enc, key, &iov, 1, &result);
    assert(!ret);
    check_result("example 4", resultbuf, cmac4);

    printf("All CMAC tests passed.\n");
    krb5_k_free_key(context, key);
    return 0;
}
