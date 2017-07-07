/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_cksum.c */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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

/* Test checksum and checksum compatability for rsa-md[4,5]-des. */

#include "k5-int.h"

#define MD5_K5BETA_COMPAT
#define MD4_K5BETA_COMPAT

#if MD == 4
#define CKTYPE CKSUMTYPE_RSA_MD4_DES
#endif

#if MD == 5
#define CKTYPE CKSUMTYPE_RSA_MD5_DES
#endif

static void
print_checksum(char *text, int number, char *message, krb5_checksum *checksum)
{
    unsigned int i;

    printf("%s MD%d checksum(\"%s\") = ", text, number, message);
    for (i=0; i<checksum->length; i++)
        printf("%02x", (unsigned char) checksum->contents[i]);
    printf("\n");
}

static void
parse_hexstring(const char *s, krb5_checksum *cksum)
{
    size_t i, len;
    unsigned int byte;
    unsigned char *cp;

    len = strlen(s);
    cp = malloc(len / 2);
    cksum->contents = cp;
    if (cp == NULL) {
        cksum->length = 0;
        return;
    }
    cksum->length = len / 2;
    for (i = 0; i + 1 < len; i += 2) {
        sscanf(&s[i], "%2x", &byte);
        *cp++ = byte;
    }
    cksum->checksum_type = CKTYPE;
    cksum->magic = KV5M_CHECKSUM;
}

/*
 * Test the checksum verification of Old Style (tm) and correct RSA-MD[4,5]-DES
 * checksums.
 */

krb5_octet testkey[8] = { 0x45, 0x01, 0x49, 0x61, 0x58, 0x19, 0x1a, 0x3d };

int
main(argc, argv)
    int argc;
    char **argv;
{
    int                   msgindex;
    krb5_boolean          valid;
    krb5_keyblock         keyblock;
    krb5_key              key;
    krb5_error_code       kret=0;
    krb5_data             plaintext;
    krb5_checksum         checksum, knowncksum;

    /* this is a terrible seed, but that's ok for the test. */

    plaintext.length = 8;
    plaintext.data = (char *) testkey;

    krb5_c_random_seed(/* XXX */ 0, &plaintext);

    keyblock.enctype = ENCTYPE_DES_CBC_CRC;
    keyblock.length = sizeof(testkey);
    keyblock.contents = testkey;

    krb5_k_create_key(NULL, &keyblock, &key);

    for (msgindex = 1; msgindex + 1 < argc; msgindex += 2) {
        plaintext.length = strlen(argv[msgindex]);
        plaintext.data = argv[msgindex];

        /* Create a checksum. */
        kret = krb5_k_make_checksum(NULL, CKTYPE, key, 0, &plaintext,
                                    &checksum);
        if (kret != 0) {
            printf("krb5_calculate_checksum choked with %d\n", kret);
            break;
        }
        print_checksum("correct", MD, argv[msgindex], &checksum);

        /* Verify it. */
        kret = krb5_k_verify_checksum(NULL, key, 0, &plaintext, &checksum,
                                      &valid);
        if (kret != 0) {
            printf("verify on new checksum choked with %d\n", kret);
            break;
        }
        if (!valid) {
            printf("verify on new checksum failed\n");
            kret = 1;
            break;
        }
        printf("Verify succeeded for \"%s\"\n", argv[msgindex]);

        /* Corrupt the checksum and see if it still verifies. */
        checksum.contents[0]++;
        kret = krb5_k_verify_checksum(NULL, key, 0, &plaintext, &checksum,
                                      &valid);
        if (kret != 0) {
            printf("verify on new checksum choked with %d\n", kret);
            break;
        }
        if (valid) {
            printf("verify on new checksum succeeded, but shouldn't have\n");
            kret = 1;
            break;
        }
        printf("Verify of bad checksum OK for \"%s\"\n", argv[msgindex]);
        free(checksum.contents);

        /* Verify a known-good checksum for this plaintext. */
        parse_hexstring(argv[msgindex+1], &knowncksum);
        if (knowncksum.contents == NULL) {
            printf("parse_hexstring failed\n");
            kret = 1;
            break;
        }
        kret = krb5_k_verify_checksum(NULL, key, 0, &plaintext, &knowncksum,
                                      &valid);
        if (kret != 0) {
            printf("verify on known checksum choked with %d\n", kret);
            break;
        }
        if (!valid) {
            printf("verify on known checksum failed\n");
            kret = 1;
            break;
        }
        printf("Verify on known checksum succeeded\n");
        free(knowncksum.contents);
    }
    if (!kret)
        printf("%d tests passed successfully for MD%d checksum\n", (argc-1)/2, MD);

    krb5_k_free_key(NULL, key);

    return(kret);
}
