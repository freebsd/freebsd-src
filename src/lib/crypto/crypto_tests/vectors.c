/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/vectors.c */
/*
 * Copyright 2001 by the Massachusetts Institute of Technology.
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

static void printhex (size_t len, const char *p)
{
    while (len--)
        printf ("%02x", 0xff & *p++);
}

static void printstringhex (const char *p) { printhex (strlen (p), p); }

static void printdata (krb5_data *d) { printhex (d->length, d->data); }

static void printkey (krb5_keyblock *k) { printhex (k->length, k->contents); }

static void test_nfold ()
{
    int i;
    static const struct {
        char *input;
        int n;
    } tests[] = {
        { "012345", 64, },
        { "password", 56, },
        { "Rough Consensus, and Running Code", 64, },
        { "password", 168, },
        { "MASSACHVSETTS INSTITVTE OF TECHNOLOGY", 192 },
        { "Q", 168 },
        { "ba", 168 },
    };
    unsigned char outbuf[192/8];

    for (i = 0; i < ASIZE (tests); i++) {
        char *p = tests[i].input;
        assert (tests[i].n / 8 <= sizeof (outbuf));
        printf ("%d-fold(\"%s\") =\n", tests[i].n, p);
        printf ("%d-fold(", tests[i].n);
        printstringhex (p);
        printf (") =\n\t");
        krb5int_nfold (8 * strlen (p), p, tests[i].n, outbuf);
        printhex (tests[i].n / 8U, outbuf);
        printf ("\n\n");
    }
}

#define JURISIC "Juri\305\241i\304\207" /* hi Miro */
#define ESZETT "\303\237"
#define GCLEF  "\360\235\204\236" /* outside BMP, woo hoo!  */

/* Some weak keys:
   {0x1f,0x1f,0x1f,0x1f,0x0e,0x0e,0x0e,0x0e},
   {0xe0,0xe0,0xe0,0xe0,0xf1,0xf1,0xf1,0xf1},
   so try to generate them. */

static void
test_mit_des_s2k ()
{
    static const struct {
        const char *pass;
        const char *salt;
    } pairs[] = {
        { "password", "ATHENA.MIT.EDUraeburn" },
        { "potatoe", "WHITEHOUSE.GOVdanny" },
        { "penny", "EXAMPLE.COMbuckaroo", },
        { GCLEF, "EXAMPLE.COMpianist" },
        { ESZETT, "ATHENA.MIT.EDU" JURISIC },
        /* These two trigger weak-key fixups.  */
        { "11119999", "AAAAAAAA" },
        { "NNNN6666", "FFFFAAAA" },
    };
    int i;

    for (i = 0; i < ASIZE (pairs); i++) {
        const char *p = pairs[i].pass;
        const char *s = pairs[i].salt;
        krb5_data pd;
        krb5_data sd;
        unsigned char key_contents[60];
        krb5_keyblock key;
        krb5_error_code r;
        char buf[80];

        key.contents = key_contents;

        pd.length = strlen (p);
        pd.data = (char *) p;
        sd.length = strlen (s);
        sd.data = (char *) s;

        assert (strlen (s) + 4 < sizeof (buf));
        snprintf (buf, sizeof (buf), "\"%s\"", s);
        printf (  "salt:     %-25s", buf);
        printhex (strlen(s), s);
        snprintf (buf, sizeof (buf), "\"%s\"", p);
        printf ("\npassword: %-25s", buf);
        printhex (strlen(p), p);
        printf ("\n");
        r = krb5int_des_string_to_key (0, &pd, &sd, 0, &key);
        printf (  "DES key:  %-25s", "");
        printhex (key.length, key.contents);
        printf ("\n\n");
    }
}

static void
test_s2k (krb5_enctype enctype)
{
    static const struct {
        const char *pass;
        const char *salt;
    } pairs[] = {
        { "password", "ATHENA.MIT.EDUraeburn" },
        { "potatoe", "WHITEHOUSE.GOVdanny" },
        { "penny", "EXAMPLE.COMbuckaroo", },
        { ESZETT, "ATHENA.MIT.EDU" JURISIC },
        { GCLEF, "EXAMPLE.COMpianist" },
    };
    int i;

    for (i = 0; i < ASIZE (pairs); i++) {
        const char *p = pairs[i].pass;
        const char *s = pairs[i].salt;
        krb5_data pd, sd;
        unsigned char key_contents[60];
        krb5_keyblock key;
        krb5_error_code r;
        char buf[80];

        pd.length = strlen (p);
        pd.data = (char *) p;
        sd.length = strlen (s);
        sd.data = (char *) s;
        key.contents = key_contents;

        assert (strlen (s) + 4 < sizeof (buf));
        snprintf (buf, sizeof(buf), "\"%s\"", s);
        printf (  "salt:\t%s\n\t", buf);
        printhex (strlen(s), s);
        snprintf (buf, sizeof(buf), "\"%s\"", p);
        printf ("\npasswd:\t%s\n\t", buf);
        printhex (strlen(p), p);
        printf ("\n");
        r = krb5_c_string_to_key (0, enctype, &pd, &sd, &key);
        printf (  "key:\t");
        printhex (key.length, key.contents);
        printf ("\n\n");
    }
}

static void test_des3_s2k () { test_s2k (ENCTYPE_DES3_CBC_SHA1); }

static void
keyToData (krb5_keyblock *k, krb5_data *d)
{
    d->length = k->length;
    d->data = k->contents;
}

void check_error (int r, int line) {
    if (r != 0) {
        fprintf (stderr, "%s:%d: %s\n", __FILE__, line,
                 error_message (r));
        exit (1);
    }
}
#define CHECK check_error(r, __LINE__)

extern struct krb5_enc_provider krb5int_enc_des3;
struct krb5_enc_provider *enc = &krb5int_enc_des3;
extern struct krb5_enc_provider krb5int_enc_aes128, krb5int_enc_aes256;

void DK (krb5_keyblock *out, krb5_keyblock *in, const krb5_data *usage) {
    krb5_error_code r;
    r = krb5int_derive_key (enc, in, out, usage, DERIVE_RFC3961);
    CHECK;
}

void DR (krb5_data *out, krb5_keyblock *in, const krb5_data *usage) {
    krb5_error_code r;
    r = krb5int_derive_random (enc, in, out, usage, DERIVE_RFC3961);
    CHECK;
}

#define KEYBYTES  21
#define KEYLENGTH 24

void test_dr_dk ()
{
    static const struct {
        unsigned char keydata[KEYLENGTH];
        int usage_len;
        unsigned char usage[8];
    } derive_tests[] = {
        {
            {
                0xdc, 0xe0, 0x6b, 0x1f, 0x64, 0xc8, 0x57, 0xa1,
                0x1c, 0x3d, 0xb5, 0x7c, 0x51, 0x89, 0x9b, 0x2c,
                0xc1, 0x79, 0x10, 0x08, 0xce, 0x97, 0x3b, 0x92,
            },
            5, { 0x00, 0x00, 0x00, 0x01, 0x55 },
        },
        {
            {
                0x5e, 0x13, 0xd3, 0x1c, 0x70, 0xef, 0x76, 0x57,
                0x46, 0x57, 0x85, 0x31, 0xcb, 0x51, 0xc1, 0x5b,
                0xf1, 0x1c, 0xa8, 0x2c, 0x97, 0xce, 0xe9, 0xf2,
            },
            5, { 0x00, 0x00, 0x00, 0x01, 0xaa },
        },
        {
            {
                0x98, 0xe6, 0xfd, 0x8a, 0x04, 0xa4, 0xb6, 0x85,
                0x9b, 0x75, 0xa1, 0x76, 0x54, 0x0b, 0x97, 0x52,
                0xba, 0xd3, 0xec, 0xd6, 0x10, 0xa2, 0x52, 0xbc,
            },
            5, { 0x00, 0x00, 0x00, 0x01, 0x55 },
        },
        {
            {
                0x62, 0x2a, 0xec, 0x25, 0xa2, 0xfe, 0x2c, 0xad,
                0x70, 0x94, 0x68, 0x0b, 0x7c, 0x64, 0x94, 0x02,
                0x80, 0x08, 0x4c, 0x1a, 0x7c, 0xec, 0x92, 0xb5,
            },
            5, { 0x00, 0x00, 0x00, 0x01, 0xaa },
        },
        {
            {
                0xd3, 0xf8, 0x29, 0x8c, 0xcb, 0x16, 0x64, 0x38,
                0xdc, 0xb9, 0xb9, 0x3e, 0xe5, 0xa7, 0x62, 0x92,
                0x86, 0xa4, 0x91, 0xf8, 0x38, 0xf8, 0x02, 0xfb,
            },
            8, { 'k', 'e', 'r', 'b', 'e', 'r', 'o', 's' },
        },
        {
            {
                0xb5, 0x5e, 0x98, 0x34, 0x67, 0xe5, 0x51, 0xb3,
                0xe5, 0xd0, 0xe5, 0xb6, 0xc8, 0x0d, 0x45, 0x76,
                0x94, 0x23, 0xa8, 0x73, 0xdc, 0x62, 0xb3, 0x0e,
            },
            7, { 'c', 'o', 'm', 'b', 'i', 'n', 'e', },
        },
        {
            {
                0xc1, 0x08, 0x16, 0x49, 0xad, 0xa7, 0x43, 0x62,
                0xe6, 0xa1, 0x45, 0x9d, 0x01, 0xdf, 0xd3, 0x0d,
                0x67, 0xc2, 0x23, 0x4c, 0x94, 0x07, 0x04, 0xda,
            },
            5, { 0x00, 0x00, 0x00, 0x01, 0x55 },
        },
        {
            {
                0x5d, 0x15, 0x4a, 0xf2, 0x38, 0xf4, 0x67, 0x13,
                0x15, 0x57, 0x19, 0xd5, 0x5e, 0x2f, 0x1f, 0x79,
                0x0d, 0xd6, 0x61, 0xf2, 0x79, 0xa7, 0x91, 0x7c,
            },
            5, { 0x00, 0x00, 0x00, 0x01, 0xaa },
        },
        {
            {
                0x79, 0x85, 0x62, 0xe0, 0x49, 0x85, 0x2f, 0x57,
                0xdc, 0x8c, 0x34, 0x3b, 0xa1, 0x7f, 0x2c, 0xa1,
                0xd9, 0x73, 0x94, 0xef, 0xc8, 0xad, 0xc4, 0x43,
            },
            5, { 0x00, 0x00, 0x00, 0x01, 0x55 },
        },
        {
            {
                0x26, 0xdc, 0xe3, 0x34, 0xb5, 0x45, 0x29, 0x2f,
                0x2f, 0xea, 0xb9, 0xa8, 0x70, 0x1a, 0x89, 0xa4,
                0xb9, 0x9e, 0xb9, 0x94, 0x2c, 0xec, 0xd0, 0x16,
            },
            5, { 0x00, 0x00, 0x00, 0x01, 0xaa },
        },
    };
    int i;

    for (i = 0; i < ASIZE(derive_tests); i++) {
#define D (derive_tests[i])
        krb5_keyblock key;
        krb5_data usage;

        unsigned char drData[KEYBYTES];
        krb5_data dr;
        unsigned char dkData[KEYLENGTH];
        krb5_keyblock dk;

        key.length = KEYLENGTH, key.contents = D.keydata;
        usage.length = D.usage_len, usage.data = D.usage;
        dr.length = KEYBYTES, dr.data = drData;
        dk.length = KEYLENGTH, dk.contents = dkData;

        printf ("key:\t"); printkey (&key); printf ("\n");
        printf ("usage:\t"); printdata (&usage); printf ("\n");
        DR (&dr, &key, &usage);
        printf ("DR:\t"); printdata (&dr); printf ("\n");
        DK (&dk, &key, &usage);
        printf ("DK:\t"); printkey (&dk); printf ("\n\n");
    }
}


static void printd (const char *descr, krb5_data *d) {
    int i, j;
    const int r = 16;

    printf("%s:", descr);

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
    d.data = k->contents;
    d.length = k->length;
    printd(descr, &d);
}


static void
test_pbkdf2()
{
    static struct {
        int count;
        char *pass;
        char *salt;
    } test[] = {
        { 1, "password", "ATHENA.MIT.EDUraeburn" },
        { 2, "password", "ATHENA.MIT.EDUraeburn" },
        { 1200, "password", "ATHENA.MIT.EDUraeburn" },
        { 5, "password", "\x12\x34\x56\x78\x78\x56\x34\x12" },
        { 1200,
          "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
          "pass phrase equals block size" },
        { 1200,
          "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
          "pass phrase exceeds block size" },
        { 50, "\xf0\x9d\x84\x9e", "EXAMPLE.COMpianist" },
    };
    unsigned char x[100];
    unsigned char x2[100];
    int j;
    krb5_error_code err;
    krb5_data d;
    krb5_keyblock k, dk;
    krb5_data usage, pass, salt;

    d.data = x;
    dk.contents = x2;

    usage.data = "kerberos";
    usage.length = 8;

    for (j = 0; j < sizeof(test)/sizeof(test[0]); j++) {
        printf("pkbdf2(count=%d, pass=\"%s\", salt=",
               test[j].count, test[j].pass);
        if (isprint(test[j].salt[0]))
            printf("\"%s\")\n", test[j].salt);
        else {
            char *s = test[j].salt;
            printf("0x");
            while (*s)
                printf("%02X", 0xff & *s++);
            printf(")\n");
        }

        d.length = 16;
        pass.data = test[j].pass;
        pass.length = strlen(pass.data);
        salt.data = test[j].salt;
        salt.length = strlen(salt.data);
        err = krb5int_pbkdf2_hmac_sha1 (&d, test[j].count, &pass, &salt);
        printd("128-bit PBKDF2 output", &d);
        enc = &krb5int_enc_aes128;
        k.contents = d.data;
        k.length = d.length;
        dk.length = d.length;
        DK (&dk, &k, &usage);
        printk("128-bit AES key",&dk);

        d.length = 32;
        err = krb5int_pbkdf2_hmac_sha1 (&d, test[j].count, &pass, &salt);
        printd("256-bit PBKDF2 output", &d);
        enc = &krb5int_enc_aes256;
        k.contents = d.data;
        k.length = d.length;
        dk.length = d.length;
        DK (&dk, &k, &usage);
        printk("256-bit AES key", &dk);

        printf("\n");
    }
}

int main (int argc, char **argv)
{
    whoami = argv[0];
    test_nfold ();
#if 0
    test_mit_des_s2k ();
    test_des3_s2k ();
    test_dr_dk ();
#endif
    test_pbkdf2();
    return 0;
}
