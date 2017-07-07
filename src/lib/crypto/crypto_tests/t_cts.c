/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_cts.c */
/*
 * Copyright 2001, 2007 by the Massachusetts Institute of Technology.
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
 * Test vectors for crypto code, matching data submitted for inclusion
 * with RFC1510bis.
 *
 * N.B.: Doesn't compile -- this file uses some routines internal to our
 * crypto library which are declared "static" and thus aren't accessible
 * without modifying the other sources.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "crypto_int.h"

#define ASIZE(ARRAY) (sizeof(ARRAY)/sizeof(ARRAY[0]))

const char *whoami;

#if 0
static void printhex (size_t len, const char *p)
{
    while (len--)
        printf ("%02x", 0xff & *p++);
}

static void printstringhex (const char *p) { printhex (strlen (p), p); }

static void printdata (krb5_data *d) { printhex (d->length, d->data); }

static void printkey (krb5_keyblock *k) { printhex (k->length, k->contents); }
#endif


#define JURISIC "Juri\305\241i\304\207" /* hi Miro */
#define ESZETT "\303\237"
#define GCLEF  "\360\235\204\236" /* outside BMP, woo hoo!  */

#if 0
static void
check_error (int r, int line) {
    if (r != 0) {
        fprintf (stderr, "%s:%d: %s\n", __FILE__, line,
                 error_message (r));
        exit (1);
    }
}
#define CHECK check_error(r, __LINE__)
#endif

static void printd (const char *descr, krb5_data *d) {
    unsigned int i, j;
    const int r = 16;

    printf("%s:", descr);

    for (i = 0; i < d->length; i += r) {
        printf("\n  %04x: ", i);
        for (j = i; j < i + r && j < d->length; j++)
            printf(" %02x", 0xff & d->data[j]);
#ifdef SHOW_TEXT
        for (; j < i + r; j++)
            printf("   ");
        printf("   ");
        for (j = i; j < i + r && j < d->length; j++) {
            int c = 0xff & d->data[j];
            printf("%c", isprint(c) ? c : '.');
        }
#endif
    }
    printf("\n");
}
static void printk(const char *descr, krb5_keyblock *k) {
    krb5_data d;
    d.data = (char *) k->contents;
    d.length = k->length;
    printd(descr, &d);
}

static void test_cts()
{
    static const char input[4*16] =
        "I would like the General Gau's Chicken, please, and wonton soup.";
    static const unsigned char aeskey[16] = "chicken teriyaki";
    static const int lengths[] = { 17, 31, 32, 47, 48, 64 };

    unsigned int i;
    char outbuf[64], encivbuf[16], decivbuf[16];
    krb5_crypto_iov iov;
    krb5_data in, enciv, deciv;
    krb5_keyblock keyblock;
    krb5_key key;
    krb5_error_code err;

    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data.data = outbuf;
    in.data = (char *)input;
    enciv.length = deciv.length = 16;
    enciv.data = encivbuf;
    deciv.data = decivbuf;
    keyblock.contents = (krb5_octet *)aeskey;
    keyblock.length = 16;
    keyblock.enctype = ENCTYPE_AES128_CTS_HMAC_SHA1_96;

    err = krb5_k_create_key(NULL, &keyblock, &key);
    if (err) {
        printf("error %ld from krb5_k_create_key\n", (long)err);
        exit(1);
    }

    memset(enciv.data, 0, 16);
    printk("AES 128-bit key", &keyblock);
    for (i = 0; i < sizeof(lengths)/sizeof(lengths[0]); i++) {
        memset(enciv.data, 0, 16);
        memset(deciv.data, 0, 16);

        printf("\n");
        iov.data.length = in.length = lengths[i];
        memcpy(outbuf, input, lengths[i]);
        printd("IV", &enciv);
        err = krb5int_aes_encrypt(key, &enciv, &iov, 1);
        if (err) {
            printf("error %ld from krb5int_aes_encrypt\n", (long)err);
            exit(1);
        }
        printd("Input", &in);
        printd("Output", &iov.data);
        printd("Next IV", &enciv);
        err = krb5int_aes_decrypt(key, &deciv, &iov, 1);
        if (err) {
            printf("error %ld from krb5int_aes_decrypt\n", (long)err);
            exit(1);
        }
        if (memcmp(outbuf, input, lengths[i]) != 0) {
            printd("Decryption result DOESN'T MATCH", &iov.data);
            exit(1);
        }
        if (memcmp(enciv.data, deciv.data, 16)) {
            printd("Decryption IV result DOESN'T MATCH", &deciv);
            exit(1);
        }
    }
    krb5_k_free_key(NULL, key);
}

int main (int argc, char **argv)
{
    whoami = argv[0];
    test_cts();
    return 0;
}
