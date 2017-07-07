/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_cksums.c - Test known checksum results */
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
 * This harness tests checksum results against known values.  With the -v flag,
 * results for all tests are displayed.  This harness only works for
 * deterministic checksums; for rsa-md4-des and rsa-md5-des, see t_cksum.c.
 */

#include "k5-int.h"

struct test {
    krb5_data plaintext;
    krb5_cksumtype sumtype;
    krb5_enctype enctype;
    krb5_keyusage usage;
    krb5_data keybits;
    krb5_data cksum;
} test_cases[] = {
    {
        { KV5M_DATA, 3, "abc" },
        CKSUMTYPE_CRC32, 0, 0, { KV5M_DATA, 0, "" },
        { KV5M_DATA, 4,
          "\xD0\x98\x65\xCA" }
    },
    {
        { KV5M_DATA, 3, "one" },
        CKSUMTYPE_RSA_MD4, 0, 0, { KV5M_DATA, 0, "" },
        { KV5M_DATA, 16,
          "\x30\x5D\xCC\x2C\x0F\xDD\x53\x39\x96\x95\x52\xC7\xB8\x99\x63\x48" }
    },
    {
        { KV5M_DATA, 19, "two three four five" },
        CKSUMTYPE_RSA_MD5, 0, 0, { KV5M_DATA, 0, "" },
        { KV5M_DATA, 16,
          "\xBA\xB5\x32\x15\x51\xE1\x08\x44\x90\x86\x96\x35\xB3\xC2\x68\x15" }
    },
    {
        { KV5M_DATA, 0, "" },
        CKSUMTYPE_NIST_SHA, 0, 0, { KV5M_DATA, 0, "" },
        { KV5M_DATA, 20,
          "\xDA\x39\xA3\xEE\x5E\x6B\x4B\x0D\x32\x55\xBF\xEF\x95\x60\x18\x90"
          "\xAF\xD8\x07\x09" }
    },
    {
        { KV5M_DATA, 9, "six seven" },
        CKSUMTYPE_HMAC_SHA1_DES3, ENCTYPE_DES3_CBC_SHA1, 2,
        { KV5M_DATA, 24,
          "\x7A\x25\xDF\x89\x92\x29\x6D\xCE\xDA\x0E\x13\x5B\xC4\x04\x6E\x23"
          "\x75\xB3\xC1\x4C\x98\xFB\xC1\x62" },
        { KV5M_DATA, 20,
          "\x0E\xEF\xC9\xC3\xE0\x49\xAA\xBC\x1B\xA5\xC4\x01\x67\x7D\x9A\xB6"
          "\x99\x08\x2B\xB4" }
    },
    {
        { KV5M_DATA, 37, "eight nine ten eleven twelve thirteen" },
        CKSUMTYPE_HMAC_SHA1_96_AES128, ENCTYPE_AES128_CTS_HMAC_SHA1_96, 3,
        { KV5M_DATA, 16,
          "\x90\x62\x43\x0C\x8C\xDA\x33\x88\x92\x2E\x6D\x6A\x50\x9F\x5B\x7A" },
        { KV5M_DATA, 12,
          "\x01\xA4\xB0\x88\xD4\x56\x28\xF6\x94\x66\x14\xE3" }
    },
    {
        { KV5M_DATA, 8, "fourteen" },
        CKSUMTYPE_HMAC_SHA1_96_AES256, ENCTYPE_AES256_CTS_HMAC_SHA1_96, 4,
        { KV5M_DATA, 32,
          "\xB1\xAE\x4C\xD8\x46\x2A\xFF\x16\x77\x05\x3C\xC9\x27\x9A\xAC\x30"
          "\xB7\x96\xFB\x81\xCE\x21\x47\x4D\xD3\xDD\xBC\xFE\xA4\xEC\x76\xD7" },
        { KV5M_DATA, 12,
          "\xE0\x87\x39\xE3\x27\x9E\x29\x03\xEC\x8E\x38\x36" }
    },
    {
        { KV5M_DATA, 15, "fifteen sixteen" },
        CKSUMTYPE_MD5_HMAC_ARCFOUR, ENCTYPE_ARCFOUR_HMAC, 5,
        { KV5M_DATA, 16,
          "\xF7\xD3\xA1\x55\xAF\x5E\x23\x8A\x0B\x7A\x87\x1A\x96\xBA\x2A\xB2" },
        { KV5M_DATA, 16,
          "\x9F\x41\xDF\x30\x49\x07\xDE\x73\x54\x47\x00\x1F\xD2\xA1\x97\xB9" }
    },
    {
        { KV5M_DATA, 34, "seventeen eighteen nineteen twenty" },
        CKSUMTYPE_HMAC_MD5_ARCFOUR, ENCTYPE_ARCFOUR_HMAC, 6,
        { KV5M_DATA, 16,
          "\xF7\xD3\xA1\x55\xAF\x5E\x23\x8A\x0B\x7A\x87\x1A\x96\xBA\x2A\xB2" },
        { KV5M_DATA, 16,
          "\xEB\x38\xCC\x97\xE2\x23\x0F\x59\xDA\x41\x17\xDC\x58\x59\xD7\xEC" }
    },
    {
        { KV5M_DATA, 11, "abcdefghijk" },
        CKSUMTYPE_CMAC_CAMELLIA128, ENCTYPE_CAMELLIA128_CTS_CMAC, 7,
        { KV5M_DATA, 16,
          "\x1D\xC4\x6A\x8D\x76\x3F\x4F\x93\x74\x2B\xCB\xA3\x38\x75\x76\xC3" },
        { KV5M_DATA, 16,
          "\x11\x78\xE6\xC5\xC4\x7A\x8C\x1A\xE0\xC4\xB9\xC7\xD4\xEB\x7B\x6B" }
    },
    {
        { KV5M_DATA, 26, "ABCDEFGHIJKLMNOPQRSTUVWXYZ" },
        CKSUMTYPE_CMAC_CAMELLIA128, ENCTYPE_CAMELLIA128_CTS_CMAC, 8,
        { KV5M_DATA, 16,
          "\x50\x27\xBC\x23\x1D\x0F\x3A\x9D\x23\x33\x3F\x1C\xA6\xFD\xBE\x7C" },
        { KV5M_DATA, 16,
          "\xD1\xB3\x4F\x70\x04\xA7\x31\xF2\x3A\x0C\x00\xBF\x6C\x3F\x75\x3A" }
    },
    {
        { KV5M_DATA, 9, "123456789" },
        CKSUMTYPE_CMAC_CAMELLIA256, ENCTYPE_CAMELLIA256_CTS_CMAC, 9,
        { KV5M_DATA, 32,
          "\xB6\x1C\x86\xCC\x4E\x5D\x27\x57\x54\x5A\xD4\x23\x39\x9F\xB7\x03"
          "\x1E\xCA\xB9\x13\xCB\xB9\x00\xBD\x7A\x3C\x6D\xD8\xBF\x92\x01\x5B" },
        { KV5M_DATA, 16,
          "\x87\xA1\x2C\xFD\x2B\x96\x21\x48\x10\xF0\x1C\x82\x6E\x77\x44\xB1" }
    },
    {
        { KV5M_DATA, 30, "!@#$%^&*()!@#$%^&*()!@#$%^&*()" },
        CKSUMTYPE_CMAC_CAMELLIA256, ENCTYPE_CAMELLIA256_CTS_CMAC, 10,
        { KV5M_DATA, 32,
          "\x32\x16\x4C\x5B\x43\x4D\x1D\x15\x38\xE4\xCF\xD9\xBE\x80\x40\xFE"
          "\x8C\x4A\xC7\xAC\xC4\xB9\x3D\x33\x14\xD2\x13\x36\x68\x14\x7A\x05" },
        { KV5M_DATA, 16,
          "\x3F\xA0\xB4\x23\x55\xE5\x2B\x18\x91\x87\x29\x4A\xA2\x52\xAB\x64" }
    },
    {
        { KV5M_DATA, 21,
          "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
          "\x10\x11\x12\x13\x14" },
        CKSUMTYPE_HMAC_SHA256_128_AES128, ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        2,
        { KV5M_DATA, 16,
          "\x37\x05\xD9\x60\x80\xC1\x77\x28\xA0\xE8\x00\xEA\xB6\xE0\xD2\x3C" },
        { KV5M_DATA, 16,
          "\xD7\x83\x67\x18\x66\x43\xD6\x7B\x41\x1C\xBA\x91\x39\xFC\x1D\xEE" }
    },
    {
        { KV5M_DATA, 21,
          "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
          "\x10\x11\x12\x13\x14" },
        CKSUMTYPE_HMAC_SHA384_192_AES256, ENCTYPE_AES256_CTS_HMAC_SHA384_192,
        2,
        { KV5M_DATA, 32,
          "\x6D\x40\x4D\x37\xFA\xF7\x9F\x9D\xF0\xD3\x35\x68\xD3\x20\x66\x98"
          "\x00\xEB\x48\x36\x47\x2E\xA8\xA0\x26\xD1\x6B\x71\x82\x46\x0C\x52" },
        { KV5M_DATA, 24,
          "\x45\xEE\x79\x15\x67\xEE\xFC\xA3\x7F\x4A\xC1\xE0\x22\x2D\xE8\x0D"
          "\x43\xC3\xBF\xA0\x66\x99\x67\x2A" }
    },
};

static void
printhex(const char *head, void *data, size_t len)
{
    size_t i;

    printf("%s", head);
    for (i = 0; i < len; i++) {
#if 0                           /* For convenience when updating test cases. */
        printf("\\x%02X", ((unsigned char*)data)[i]);
#else
        printf("%02X", ((unsigned char*)data)[i]);
        if (i % 16 == 15 && i + 1 < len)
            printf("\n%*s", (int)strlen(head), "");
        else if (i + 1 < len)
            printf(" ");
#endif
    }
    printf("\n");
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context = NULL;
    size_t i;
    struct test *test;
    krb5_keyblock kb, *kbp;
    krb5_checksum cksum;
    krb5_cksumtype mtype;
    krb5_boolean valid, verbose = FALSE;
    int status = 0;

    if (argc >= 2 && strcmp(argv[1], "-v") == 0)
        verbose = TRUE;
    for (i = 0; i < sizeof(test_cases) / sizeof(*test_cases); i++) {
        test = &test_cases[i];
        if (test->enctype != 0) {
            kb.magic = KV5M_KEYBLOCK;
            kb.enctype = test->enctype;
            kb.length = test->keybits.length;
            kb.contents = (unsigned char *)test->keybits.data;
            kbp = &kb;
        } else
            kbp = NULL;
        ret = krb5_c_make_checksum(context, test->sumtype, kbp, test->usage,
                                   &test->plaintext, &cksum);
        assert(!ret);
        if (verbose) {
            char buf[64];
            krb5_cksumtype_to_string(test->sumtype, buf, sizeof(buf));
            printf("\nTest %d:\n", (int)i);
            printf("Plaintext: %.*s\n", (int)test->plaintext.length,
                   test->plaintext.data);
            printf("Checksum type: %s\n", buf);
            if (test->enctype != 0) {
                krb5_enctype_to_name(test->enctype, FALSE, buf, sizeof(buf));
                printf("Enctype: %s\n", buf);
                printhex("Key: ", test->keybits.data, test->keybits.length);
                printf("Key usage: %d\n", (int)test->usage);
            }
            printhex("Checksum: ", cksum.contents, cksum.length);
        }
        if (test->cksum.length != cksum.length ||
            memcmp(test->cksum.data, cksum.contents, cksum.length) != 0) {
            printf("derive test %d failed\n", (int)i);
            status = 1;
            if (!verbose)
                break;
        }

        /* Test that the checksum verifies successfully. */
        ret = krb5_c_verify_checksum(context, kbp, test->usage,
                                     &test->plaintext, &cksum, &valid);
        assert(!ret);
        if (!valid) {
            printf("test %d verify failed\n", (int)i);
            status = 1;
            if (!verbose)
                break;
        }

        if (kbp != NULL) {
            ret = krb5int_c_mandatory_cksumtype(context, kbp->enctype, &mtype);
            assert(!ret);
            if (test->sumtype == mtype) {
                /* Test that a checksum type of 0 uses the mandatory checksum
                 * type for the key. */
                cksum.checksum_type = 0;
                ret = krb5_c_verify_checksum(context, kbp, test->usage,
                                             &test->plaintext, &cksum, &valid);
                assert(!ret && valid);
            }
        }

        krb5_free_checksum_contents(context, &cksum);
    }
    return status;
}
