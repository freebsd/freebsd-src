/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_hmac.c */
/*
 * Copyright 2001,2002 by the Massachusetts Institute of Technology.
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
 * Test vectors for HMAC-MD5 and HMAC-SHA1 (placeholder only).
 * Tests taken from RFC 2202.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "crypto_int.h"

#define ASIZE(ARRAY) (sizeof(ARRAY)/sizeof(ARRAY[0]))

const char *whoami;

static void keyToData (krb5_keyblock *k, krb5_data *d) {
    d->length = k->length;
    d->data = (char *) k->contents;
}

#if 0
static void check_error (int r, int line) {
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

    printf("%s (%d bytes):", descr, d->length);

    for (i = 0; i < d->length; i += r) {
        printf("\n  %04x: ", i);
        for (j = i; j < i + r && j < d->length; j++)
            printf(" %02x", 0xff & d->data[j]);
        for (; j < i + r; j++)
            printf("   ");
        printf("   ");
        for (j = i; j < i + r && j < d->length; j++) {
            int c = 0xff & d->data[j];
            printf("%c", isprint(c) ? c : '.');
        }
    }
    printf("\n");
}
static void printk(const char *descr, krb5_keyblock *k) {
    krb5_data d;
    keyToData(k,&d);
    printd(descr, &d);
}



struct hmac_test {
    int key_len;
    unsigned char key[180];
    int data_len;
    unsigned char data[80];
    const char *hexdigest;
};

static krb5_error_code hmac1(const struct krb5_hash_provider *h,
                             krb5_keyblock *key,
                             krb5_data *in, krb5_data *out)
{
    char tmp[40];
    size_t blocksize, hashsize;
    krb5_error_code err;
    krb5_key k;
    krb5_crypto_iov iov;
    krb5_data d;

    printk(" test key", key);
    blocksize = h->blocksize;
    hashsize = h->hashsize;
    if (hashsize > sizeof(tmp))
        abort();
    if (key->length > blocksize) {
        iov.flags = KRB5_CRYPTO_TYPE_DATA;
        iov.data = make_data(key->contents, key->length);
        d = make_data(tmp, hashsize);
        err = h->hash(&iov, 1, &d);
        if (err) {
            com_err(whoami, err, "hashing key before calling hmac");
            exit(1);
        }
        key->length = d.length;
        key->contents = (krb5_octet *) d.data;
        printk(" pre-hashed key", key);
    }
    printd(" hmac input", in);
    krb5_k_create_key(NULL, key, &k);
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = *in;
    err = krb5int_hmac(h, k, &iov, 1, out);
    krb5_k_free_key(NULL, k);
    if (err == 0)
        printd(" hmac output", out);
    return err;
}

static void test_hmac()
{
    krb5_keyblock key;
    krb5_data in, out;
    char outbuf[20];
    char stroutbuf[80];
    krb5_error_code err;
    unsigned int i, j;
    int lose = 0;
    struct k5buf buf;

    /* RFC 2202 test vector.  */
    static const struct hmac_test md5tests[] = {
        {
            16, {
                0xb, 0xb, 0xb, 0xb, 0xb, 0xb, 0xb, 0xb,
                0xb, 0xb, 0xb, 0xb, 0xb, 0xb, 0xb, 0xb,
            },
            8, "Hi There",
            "0x9294727a3638bb1c13f48ef8158bfc9d"
        },

        {
            4, "Jefe",
            28, "what do ya want for nothing?",
            "0x750c783e6ab0b503eaa86e310a5db738"
        },

        {
            16, {
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa
            },
            50, {
                0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
                0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
                0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
                0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
                0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            },
            "0x56be34521d144c88dbb8c733f0e8b3f6"
        },

        {
            25, {
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
                0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
                0x15, 0x16, 0x17, 0x18, 0x19
            },
            50, {
                0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
                0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
                0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
                0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
                0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
            },
            "0x697eaf0aca3a3aea3a75164746ffaa79"
        },

        {
            16, {
                0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
                0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c
            },
            20, "Test With Truncation",
            "0x56461ef2342edc00f9bab995690efd4c"
        },

        {
            80, {
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
            },
            54, "Test Using Larger Than Block-Size Key - Hash Key First",
            "0x6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd"
        },

        {
            80, {
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
            },
            73,
            "Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
            "0x6f630fad67cda0ee1fb1f562db3aa53e"
        },
    };

    for (i = 0; i < sizeof(md5tests)/sizeof(md5tests[0]); i++) {
        key.contents = (krb5_octet *)md5tests[i].key;
        key.length = md5tests[i].key_len;
        in = make_data((char *)md5tests[i].data, md5tests[i].data_len);

        out.data = outbuf;
        out.length = 20;
        printf("\nTest #%d:\n", i+1);
        err = hmac1(&krb5int_hash_md5, &key, &in, &out);
        if (err) {
            com_err(whoami, err, "computing hmac");
            exit(1);
        }

        k5_buf_init_fixed(&buf, stroutbuf, sizeof(stroutbuf));
        k5_buf_add(&buf, "0x");
        for (j = 0; j < out.length; j++)
            k5_buf_add_fmt(&buf, "%02x", 0xff & outbuf[j]);
        if (k5_buf_status(&buf) != 0)
            abort();
        if (strcmp(stroutbuf, md5tests[i].hexdigest)) {
            printf("*** CHECK FAILED!\n"
                   "\tReturned: %s.\n"
                   "\tExpected: %s.\n", stroutbuf, md5tests[i].hexdigest);
            lose++;
        } else
            printf("Matches expected result.\n");
    }

    /* Do again with SHA-1 tests....  */

    if (lose) {
        printf("%d failures; exiting.\n", lose);
        exit(1);
    }
}


int main (int argc, char **argv)
{
    whoami = argv[0];
    test_hmac();
    return 0;
}
