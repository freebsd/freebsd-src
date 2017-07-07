/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_derive.c - Test harness for key derivation */
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
 * This harness detects changes in key derivation results using known values.
 * With the -v flag, results for all tests are displayed.
 */

#include "crypto_int.h"

struct test {
    krb5_enctype enctype;
    krb5_data inkey;
    krb5_data constant;
    enum deriv_alg alg;
    krb5_data expected_key;
} test_cases[] = {
    /* Kc, Ke, Kei for a DES3 key */
    {
        ENCTYPE_DES3_CBC_SHA1,
        { KV5M_DATA, 24,
          "\x85\x0B\xB5\x13\x58\x54\x8C\xD0\x5E\x86\x76\x8C\x31\x3E\x3B\xFE"
          "\xF7\x51\x19\x37\xDC\xF7\x2C\x3E" },
        { KV5M_DATA, 5, "\0\0\0\2\x99" },
        DERIVE_RFC3961,
        { KV5M_DATA, 24,
          "\xF7\x8C\x49\x6D\x16\xE6\xC2\xDA\xE0\xE0\xB6\xC2\x40\x57\xA8\x4C"
          "\x04\x26\xAE\xEF\x26\xFD\x6D\xCE" }
    },
    {
        ENCTYPE_DES3_CBC_SHA1,
        { KV5M_DATA, 24,
          "\x85\x0B\xB5\x13\x58\x54\x8C\xD0\x5E\x86\x76\x8C\x31\x3E\x3B\xFE"
          "\xF7\x51\x19\x37\xDC\xF7\x2C\x3E" },
        { KV5M_DATA, 5, "\0\0\0\2\xAA" },
        DERIVE_RFC3961,
        { KV5M_DATA, 24,
          "\x5B\x57\x23\xD0\xB6\x34\xCB\x68\x4C\x3E\xBA\x52\x64\xE9\xA7\x0D"
          "\x52\xE6\x83\x23\x1A\xD3\xC4\xCE" }
    },
    {
        ENCTYPE_DES3_CBC_SHA1,
        { KV5M_DATA, 24,
          "\x85\x0B\xB5\x13\x58\x54\x8C\xD0\x5E\x86\x76\x8C\x31\x3E\x3B\xFE"
          "\xF7\x51\x19\x37\xDC\xF7\x2C\x3E" },
        { KV5M_DATA, 5, "\0\0\0\2\x55" },
        DERIVE_RFC3961,
        { KV5M_DATA, 24,
          "\xA7\x7C\x94\x98\x0E\x9B\x73\x45\xA8\x15\x25\xC4\x23\xA7\x37\xCE"
          "\x67\xF4\xCD\x91\xB6\xB3\xDA\x45" }
    },

    /* Kc, Ke, Ki for an AES-128 key */
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 16,
          "\x42\x26\x3C\x6E\x89\xF4\xFC\x28\xB8\xDF\x68\xEE\x09\x79\x9F\x15" },
        { KV5M_DATA, 5, "\0\0\0\2\x99" },
        DERIVE_RFC3961,
        { KV5M_DATA, 16,
          "\x34\x28\x0A\x38\x2B\xC9\x27\x69\xB2\xDA\x2F\x9E\xF0\x66\x85\x4B" }
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 16,
          "\x42\x26\x3C\x6E\x89\xF4\xFC\x28\xB8\xDF\x68\xEE\x09\x79\x9F\x15" },
        { KV5M_DATA, 5, "\0\0\0\2\xAA" },
        DERIVE_RFC3961,
        { KV5M_DATA, 16,
          "\x5B\x14\xFC\x4E\x25\x0E\x14\xDD\xF9\xDC\xCF\x1A\xF6\x67\x4F\x53" }
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 16,
          "\x42\x26\x3C\x6E\x89\xF4\xFC\x28\xB8\xDF\x68\xEE\x09\x79\x9F\x15" },
        { KV5M_DATA, 5, "\0\0\0\2\x55" },
        DERIVE_RFC3961,
        { KV5M_DATA, 16,
          "\x4E\xD3\x10\x63\x62\x16\x84\xF0\x9A\xE8\xD8\x99\x91\xAF\x3E\x8F" }
    },

    /* Kc, Ke, Ki for an AES-256 key */
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 32,
          "\xFE\x69\x7B\x52\xBC\x0D\x3C\xE1\x44\x32\xBA\x03\x6A\x92\xE6\x5B"
          "\xBB\x52\x28\x09\x90\xA2\xFA\x27\x88\x39\x98\xD7\x2A\xF3\x01\x61" },
        { KV5M_DATA, 5, "\0\0\0\2\x99" },
        DERIVE_RFC3961,
        { KV5M_DATA, 32,
          "\xBF\xAB\x38\x8B\xDC\xB2\x38\xE9\xF9\xC9\x8D\x6A\x87\x83\x04\xF0"
          "\x4D\x30\xC8\x25\x56\x37\x5A\xC5\x07\xA7\xA8\x52\x79\x0F\x46\x74" }
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 32,
          "\xFE\x69\x7B\x52\xBC\x0D\x3C\xE1\x44\x32\xBA\x03\x6A\x92\xE6\x5B"
          "\xBB\x52\x28\x09\x90\xA2\xFA\x27\x88\x39\x98\xD7\x2A\xF3\x01\x61" },
        { KV5M_DATA, 5, "\0\0\0\2\xAA" },
        DERIVE_RFC3961,
        { KV5M_DATA, 32,
          "\xC7\xCF\xD9\xCD\x75\xFE\x79\x3A\x58\x6A\x54\x2D\x87\xE0\xD1\x39"
          "\x6F\x11\x34\xA1\x04\xBB\x1A\x91\x90\xB8\xC9\x0A\xDA\x3D\xDF\x37" }
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 32,
          "\xFE\x69\x7B\x52\xBC\x0D\x3C\xE1\x44\x32\xBA\x03\x6A\x92\xE6\x5B"
          "\xBB\x52\x28\x09\x90\xA2\xFA\x27\x88\x39\x98\xD7\x2A\xF3\x01\x61" },
        { KV5M_DATA, 5, "\0\0\0\2\x55" },
        DERIVE_RFC3961,
        { KV5M_DATA, 32,
          "\x97\x15\x1B\x4C\x76\x94\x50\x63\xE2\xEB\x05\x29\xDC\x06\x7D\x97"
          "\xD7\xBB\xA9\x07\x76\xD8\x12\x6D\x91\xF3\x4F\x31\x01\xAE\xA8\xBA" }
    },

    /* Kc, Ke, Ki for a Camellia-128 key */
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        { KV5M_DATA, 16,
          "\x57\xD0\x29\x72\x98\xFF\xD9\xD3\x5D\xE5\xA4\x7F\xB4\xBD\xE2\x4B" },
        { KV5M_DATA, 5, "\0\0\0\2\x99" },
        DERIVE_SP800_108_CMAC,
        { KV5M_DATA, 16,
          "\xD1\x55\x77\x5A\x20\x9D\x05\xF0\x2B\x38\xD4\x2A\x38\x9E\x5A\x56" }
    },
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        { KV5M_DATA, 16,
          "\x57\xD0\x29\x72\x98\xFF\xD9\xD3\x5D\xE5\xA4\x7F\xB4\xBD\xE2\x4B" },
        { KV5M_DATA, 5, "\0\0\0\2\xAA" },
        DERIVE_SP800_108_CMAC,
        { KV5M_DATA, 16,
          "\x64\xDF\x83\xF8\x5A\x53\x2F\x17\x57\x7D\x8C\x37\x03\x57\x96\xAB" }
    },
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        { KV5M_DATA, 16,
          "\x57\xD0\x29\x72\x98\xFF\xD9\xD3\x5D\xE5\xA4\x7F\xB4\xBD\xE2\x4B" },
        { KV5M_DATA, 5, "\0\0\0\2\x55" },
        DERIVE_SP800_108_CMAC,
        { KV5M_DATA, 16,
          "\x3E\x4F\xBD\xF3\x0F\xB8\x25\x9C\x42\x5C\xB6\xC9\x6F\x1F\x46\x35" }
    },

    /* Kc, Ke, Ki for a Camellia-256 key */
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        { KV5M_DATA, 32,
          "\xB9\xD6\x82\x8B\x20\x56\xB7\xBE\x65\x6D\x88\xA1\x23\xB1\xFA\xC6"
          "\x82\x14\xAC\x2B\x72\x7E\xCF\x5F\x69\xAF\xE0\xC4\xDF\x2A\x6D\x2C" },
        { KV5M_DATA, 5, "\0\0\0\2\x99" },
        DERIVE_SP800_108_CMAC,
        { KV5M_DATA, 32,
          "\xE4\x67\xF9\xA9\x55\x2B\xC7\xD3\x15\x5A\x62\x20\xAF\x9C\x19\x22"
          "\x0E\xEE\xD4\xFF\x78\xB0\xD1\xE6\xA1\x54\x49\x91\x46\x1A\x9E\x50" }
    },
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        { KV5M_DATA, 32,
          "\xB9\xD6\x82\x8B\x20\x56\xB7\xBE\x65\x6D\x88\xA1\x23\xB1\xFA\xC6"
          "\x82\x14\xAC\x2B\x72\x7E\xCF\x5F\x69\xAF\xE0\xC4\xDF\x2A\x6D\x2C" },
        { KV5M_DATA, 5, "\0\0\0\2\xAA" },
        DERIVE_SP800_108_CMAC,
        { KV5M_DATA, 32,
          "\x41\x2A\xEF\xC3\x62\xA7\x28\x5F\xC3\x96\x6C\x6A\x51\x81\xE7\x60"
          "\x5A\xE6\x75\x23\x5B\x6D\x54\x9F\xBF\xC9\xAB\x66\x30\xA4\xC6\x04" }
    },
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        { KV5M_DATA, 32,
          "\xB9\xD6\x82\x8B\x20\x56\xB7\xBE\x65\x6D\x88\xA1\x23\xB1\xFA\xC6"
          "\x82\x14\xAC\x2B\x72\x7E\xCF\x5F\x69\xAF\xE0\xC4\xDF\x2A\x6D\x2C" },
        { KV5M_DATA, 5, "\0\0\0\2\x55" },
        DERIVE_SP800_108_CMAC,
        { KV5M_DATA, 32,
          "\xFA\x62\x4F\xA0\xE5\x23\x99\x3F\xA3\x88\xAE\xFD\xC6\x7E\x67\xEB"
          "\xCD\x8C\x08\xE8\xA0\x24\x6B\x1D\x73\xB0\xD1\xDD\x9F\xC5\x82\xB0" }
    },

    /* Kc, Ke, Ki for an aes128-sha2 key. */
    {
        ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        { KV5M_DATA, 16,
          "\x37\x05\xD9\x60\x80\xC1\x77\x28\xA0\xE8\x00\xEA\xB6\xE0\xD2\x3C" },
        { KV5M_DATA, 5, "\0\0\0\2\x99" },
        DERIVE_SP800_108_HMAC,
        { KV5M_DATA, 16,
          "\xB3\x1A\x01\x8A\x48\xF5\x47\x76\xF4\x03\xE9\xA3\x96\x32\x5D\xC3" }
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        { KV5M_DATA, 16,
          "\x37\x05\xD9\x60\x80\xC1\x77\x28\xA0\xE8\x00\xEA\xB6\xE0\xD2\x3C" },
        { KV5M_DATA, 5, "\0\0\0\2\xAA" },
        DERIVE_SP800_108_HMAC,
        { KV5M_DATA, 16,
          "\x9B\x19\x7D\xD1\xE8\xC5\x60\x9D\x6E\x67\xC3\xE3\x7C\x62\xC7\x2E" }
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        { KV5M_DATA, 16,
          "\x37\x05\xD9\x60\x80\xC1\x77\x28\xA0\xE8\x00\xEA\xB6\xE0\xD2\x3C" },
        { KV5M_DATA, 5, "\0\0\0\2\x55" },
        DERIVE_SP800_108_HMAC,
        { KV5M_DATA, 16,
          "\x9F\xDA\x0E\x56\xAB\x2D\x85\xE1\x56\x9A\x68\x86\x96\xC2\x6A\x6C" }
    },

    /* Kc, Ke, Ki for an aes256-sha2 key. */
    {
        ENCTYPE_AES256_CTS_HMAC_SHA384_192,
        { KV5M_DATA, 32,
          "\x6D\x40\x4D\x37\xFA\xF7\x9F\x9D\xF0\xD3\x35\x68\xD3\x20\x66\x98"
          "\x00\xEB\x48\x36\x47\x2E\xA8\xA0\x26\xD1\x6B\x71\x82\x46\x0C\x52" },
        { KV5M_DATA, 5, "\0\0\0\2\x99" },
        DERIVE_SP800_108_HMAC,
        { KV5M_DATA, 24,
          "\xEF\x57\x18\xBE\x86\xCC\x84\x96\x3D\x8B\xBB\x50\x31\xE9\xF5\xC4"
          "\xBA\x41\xF2\x8F\xAF\x69\xE7\x3D" }
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA384_192,
        { KV5M_DATA, 32,
          "\x6D\x40\x4D\x37\xFA\xF7\x9F\x9D\xF0\xD3\x35\x68\xD3\x20\x66\x98"
          "\x00\xEB\x48\x36\x47\x2E\xA8\xA0\x26\xD1\x6B\x71\x82\x46\x0C\x52" },
        { KV5M_DATA, 5, "\0\0\0\2\xAA" },
        DERIVE_SP800_108_HMAC,
        { KV5M_DATA, 32,
          "\x56\xAB\x22\xBE\xE6\x3D\x82\xD7\xBC\x52\x27\xF6\x77\x3F\x8E\xA7"
          "\xA5\xEB\x1C\x82\x51\x60\xC3\x83\x12\x98\x0C\x44\x2E\x5C\x7E\x49" }
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA384_192,
        { KV5M_DATA, 32,
          "\x6D\x40\x4D\x37\xFA\xF7\x9F\x9D\xF0\xD3\x35\x68\xD3\x20\x66\x98"
          "\x00\xEB\x48\x36\x47\x2E\xA8\xA0\x26\xD1\x6B\x71\x82\x46\x0C\x52" },
        { KV5M_DATA, 5, "\0\0\0\2\x55" },
        DERIVE_SP800_108_HMAC,
        { KV5M_DATA, 24,
          "\x69\xB1\x65\x14\xE3\xCD\x8E\x56\xB8\x20\x10\xD5\xC7\x30\x12\xB6"
          "\x22\xC4\xD0\x0F\xFC\x23\xED\x1F" }
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

static const struct krb5_enc_provider *
get_enc_provider(krb5_enctype enctype)
{
    switch (enctype) {
    case ENCTYPE_DES3_CBC_SHA1:              return &krb5int_enc_des3;
    case ENCTYPE_AES128_CTS_HMAC_SHA1_96:    return &krb5int_enc_aes128;
    case ENCTYPE_AES256_CTS_HMAC_SHA1_96:    return &krb5int_enc_aes256;
    case ENCTYPE_CAMELLIA128_CTS_CMAC:       return &krb5int_enc_camellia128;
    case ENCTYPE_CAMELLIA256_CTS_CMAC:       return &krb5int_enc_camellia256;
    case ENCTYPE_AES128_CTS_HMAC_SHA256_128: return &krb5int_enc_aes128;
    case ENCTYPE_AES256_CTS_HMAC_SHA384_192: return &krb5int_enc_aes256;
    }
    abort();
}

static const struct krb5_hash_provider *
get_hash_provider(krb5_enctype enctype)
{
    switch (enctype) {
    case ENCTYPE_AES128_CTS_HMAC_SHA256_128: return &krb5int_hash_sha256;
    case ENCTYPE_AES256_CTS_HMAC_SHA384_192: return &krb5int_hash_sha384;
    }
    return NULL;
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context = NULL;
    size_t i;
    struct test *test;
    krb5_keyblock kb;
    krb5_key inkey = NULL, key = NULL;
    krb5_data rnd = empty_data(), outcmp;
    const struct krb5_enc_provider *enc;
    const struct krb5_hash_provider *hash;
    krb5_boolean verbose = FALSE;
    int status = 0;

    if (argc >= 2 && strcmp(argv[1], "-v") == 0)
        verbose = TRUE;
    for (i = 0; i < sizeof(test_cases) / sizeof(*test_cases); i++) {
        test = &test_cases[i];
        kb.magic = KV5M_KEYBLOCK;
        kb.enctype = test->enctype;
        kb.length = test->inkey.length;
        kb.contents = (unsigned char *)test->inkey.data;
        ret = krb5_k_create_key(context, &kb, &inkey);
        assert(!ret);
        enc = get_enc_provider(test->enctype);
        hash = get_hash_provider(test->enctype);
        if (test->expected_key.length == enc->keylength) {
            ret = krb5int_derive_key(enc, hash, inkey, &key, &test->constant,
                                     test->alg);
            assert(!ret);
            outcmp = make_data(key->keyblock.contents, key->keyblock.length);
        } else {
            ret = alloc_data(&rnd, test->expected_key.length);
            assert(!ret);
            ret = krb5int_derive_random(enc, hash, inkey, &rnd,
                                        &test->constant, test->alg);
            assert(!ret);
            outcmp = rnd;
        }
        if (verbose) {
            char buf[64];
            krb5_enctype_to_name(test->enctype, FALSE, buf, sizeof(buf));
            printf("\nTest %d:\n", (int)i);
            printf("Enctype: %s\n", buf);
            printhex("Input key: ", inkey->keyblock.contents,
                     inkey->keyblock.length);
            printhex("Constant: ", test->constant.data, test->constant.length);
            printhex("Output: ", outcmp.data, outcmp.length);
        }
        assert(outcmp.length == test->expected_key.length);
        if (memcmp(outcmp.data, test->expected_key.data, outcmp.length) != 0) {
            printf("derive test %d failed\n", (int)i);
            status = 1;
            if (!verbose)
                break;
        }

        krb5_k_free_key(context, inkey);
        krb5_k_free_key(context, key);
        zapfree(rnd.data, rnd.length);
        inkey = key = NULL;
        rnd = empty_data();
    }
    return status;
}
