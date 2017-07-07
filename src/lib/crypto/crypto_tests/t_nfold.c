/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_nfold.c - Test nfold implementation correctness */
/*
 * Copyright 1988, 1990 by the Massachusetts Institute of Technology.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "crypto_int.h"

#define ASIZE(ARRAY) (sizeof(ARRAY)/sizeof(ARRAY[0]))

static void printhex (size_t len, const unsigned char *p)
{
    while (len--)
        printf ("%02x", 0xff & *p++);
}

static void printstringhex (const unsigned char *p) {
    printhex (strlen ((const char *) p), p);
}

static void rfc_tests ()
{
    unsigned i;
    struct {
        char *input;
        unsigned int n;
        unsigned char exp[192/8];
    } tests[] = {
        { "012345", 64,
          { 0xbe,0x07,0x26,0x31,0x27,0x6b,0x19,0x55, }
        },
        { "password", 56,
          { 0x78,0xa0,0x7b,0x6c,0xaf,0x85,0xfa, }
        },
        { "Rough Consensus, and Running Code", 64,
          { 0xbb,0x6e,0xd3,0x08,0x70,0xb7,0xf0,0xe0, }
        },
        { "password", 168,
          { 0x59,0xe4,0xa8,0xca,0x7c,0x03,0x85,0xc3,
            0xc3,0x7b,0x3f,0x6d,0x20,0x00,0x24,0x7c,
            0xb6,0xe6,0xbd,0x5b,0x3e, }
        },
        { "MASSACHVSETTS INSTITVTE OF TECHNOLOGY", 192,
          { 0xdb,0x3b,0x0d,0x8f,0x0b,0x06,0x1e,0x60,
            0x32,0x82,0xb3,0x08,0xa5,0x08,0x41,0x22,
            0x9a,0xd7,0x98,0xfa,0xb9,0x54,0x0c,0x1b, }
        },
    };
    unsigned char outbuf[192/8];

    printf ("RFC tests:\n");
    for (i = 0; i < ASIZE (tests); i++) {
        unsigned char *p = (unsigned char *) tests[i].input;
        assert (tests[i].n / 8 <= sizeof (outbuf));
        krb5int_nfold (8 * strlen ((char *) p), p, tests[i].n, outbuf);
        printf ("%d-fold(\"%s\") =\n", tests[i].n, p);
        printf ("%d-fold(", tests[i].n);
        printstringhex (p);
        printf (") =\n\t");
        printhex (tests[i].n / 8, outbuf);
        printf ("\n\n");
        if (memcmp (outbuf, tests[i].exp, tests[i].n/8) != 0) {
            printf ("wrong value! expected:\n\t");
            printhex (tests[i].n / 8, tests[i].exp);
            exit (1);
        }
    }
}

static void fold_kerberos(unsigned int nbytes)
{
    unsigned char cipher_text[300];
    unsigned int j;

    if (nbytes > 300)
        abort();

    printf("%d-fold(\"kerberos\") =\n\t", nbytes*8);
    krb5int_nfold(64, (unsigned char *) "kerberos", 8*nbytes, cipher_text);
    for (j=0; j<nbytes; j++)
        printf("%s%02x", (j&3) ? "" : " ", cipher_text[j]);
    printf("\n");
}

unsigned char *nfold_in[] = {
    (unsigned char *) "basch",
    (unsigned char *) "eichin",
    (unsigned char *) "sommerfeld",
    (unsigned char *) "MASSACHVSETTS INSTITVTE OF TECHNOLOGY" };

unsigned char nfold_192[4][24] = {
    { 0x1a, 0xab, 0x6b, 0x42, 0x96, 0x4b, 0x98, 0xb2, 0x1f, 0x8c, 0xde, 0x2d,
      0x24, 0x48, 0xba, 0x34, 0x55, 0xd7, 0x86, 0x2c, 0x97, 0x31, 0x64, 0x3f },
    { 0x65, 0x69, 0x63, 0x68, 0x69, 0x6e, 0x4b, 0x73, 0x2b, 0x4b, 0x1b, 0x43,
      0xda, 0x1a, 0x5b, 0x99, 0x5a, 0x58, 0xd2, 0xc6, 0xd0, 0xd2, 0xdc, 0xca },
    { 0x2f, 0x7a, 0x98, 0x55, 0x7c, 0x6e, 0xe4, 0xab, 0xad, 0xf4, 0xe7, 0x11,
      0x92, 0xdd, 0x44, 0x2b, 0xd4, 0xff, 0x53, 0x25, 0xa5, 0xde, 0xf7, 0x5c },
    { 0xdb, 0x3b, 0x0d, 0x8f, 0x0b, 0x06, 0x1e, 0x60, 0x32, 0x82, 0xb3, 0x08,
      0xa5, 0x08, 0x41, 0x22, 0x9a, 0xd7, 0x98, 0xfa, 0xb9, 0x54, 0x0c, 0x1b }
};

int
main(argc, argv)
    int argc;
    char *argv[];
{
    unsigned char cipher_text[64];
    unsigned int i, j;

    printf("N-fold\n");
    for (i=0; i<sizeof(nfold_in)/sizeof(char *); i++) {
        printf("\tInput:\t\"%.*s\"\n", (int) strlen((char *) nfold_in[i]),
               nfold_in[i]);
        printf("\t192-Fold:\t");
        krb5int_nfold(strlen((char *) nfold_in[i])*8, nfold_in[i], 24*8,
                      cipher_text);
        for (j=0; j<24; j++)
            printf("%s%02x", (j&3) ? "" : " ", cipher_text[j]);
        printf("\n");
        if (memcmp(cipher_text, nfold_192[i], 24)) {
            printf("verify: error in n-fold\n");
            exit(-1);
        };
    }
    rfc_tests ();

    printf("verify: N-fold is correct\n\n");

    fold_kerberos(8);
    fold_kerberos(16);
    fold_kerberos(21);
    fold_kerberos(32);

    exit(0);
}
