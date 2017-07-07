/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_pkcs5.c */
/*
 * Copyright 2002 by the Massachusetts Institute of Technology.
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

/* Test vectors for PBKDF2 (from PKCS #5v2), based on RFC 3211. */

#include "k5-int.h"

static void printhex (size_t len, const char *p)
{
    while (len--)
        printf (" %02X", 0xff & *p++);
}

static void printdata (krb5_data *d) {
    printhex (d->length, d->data);
}

static void test_pbkdf2_rfc3211()
{
    char x[100];
    krb5_error_code err;
    krb5_data d, pass, salt;
    int i;

    /* RFC 3211 test cases.  */
    static const struct {
        const char *pass;
        const char *salt;
        unsigned int count;
        size_t len;
        const unsigned char expected[24];
    } t[] = {
        { "password", "\x12\x34\x56\x78\x78\x56\x34\x12", 5, 8,
          { 0xD1, 0xDA, 0xA7, 0x86, 0x15, 0xF2, 0x87, 0xE6 } },
        { "All n-entities must communicate with other "
          "n-entities via n-1 entiteeheehees",
          "\x12\x34\x56\x78\x78\x56\x34\x12", 500, 24,
          { 0x6A, 0x89, 0x70, 0xBF, 0x68, 0xC9, 0x2C, 0xAE,
            0xA8, 0x4A, 0x8D, 0xF2, 0x85, 0x10, 0x85, 0x86,
            0x07, 0x12, 0x63, 0x80, 0xCC, 0x47, 0xAB, 0x2D } },
    };

    d.data = x;
    printf("RFC 3211 test of PBKDF#2\n");

    for (i = 0; i < sizeof(t)/sizeof(t[0]); i++) {

        printf("pkbdf2(iter_count=%d, dklen=%d (%d bytes), salt=12 34 56 78 78 56 34 12,\n"
               "       pass=%s):\n  ->",
               t[i].count, t[i].len * 8, t[i].len, t[i].pass);

        d.length = t[i].len;
        pass.data = t[i].pass;
        pass.length = strlen(pass.data);
        salt.data = t[i].salt;
        salt.length = strlen(salt.data);
        err = krb5int_pbkdf2_hmac_sha1 (&d, t[i].count, &pass, &salt);
        if (err) {
            printf("error in computing pbkdf2: %s\n", error_message(err));
            exit(1);
        }
        printdata(&d);
        if (!memcmp(x, t[i].expected, t[i].len))
            printf("\nTest passed.\n\n");
        else {
            printf("\n*** CHECK FAILED!\n");
            exit(1);
        }
    }
}

int main ()
{
    test_pbkdf2_rfc3211();
    return 0;
}
