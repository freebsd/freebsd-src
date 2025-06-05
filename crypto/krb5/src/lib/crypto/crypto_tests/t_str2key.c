/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_str2key.c - String-to-key test vectors */
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

#include "k5-int.h"

struct test {
    krb5_enctype enctype;
    char *string;
    krb5_data salt;
    krb5_data params;
    krb5_data expected_key;
    krb5_error_code expected_err;
    krb5_boolean allow_weak;
} test_cases[] = {
    /* Test vectors from RFC 3961 appendix A.4. */
    {
        ENCTYPE_DES3_CBC_SHA1,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 0, NULL },
        { KV5M_DATA, 24, "\x85\x0B\xB5\x13\x58\x54\x8C\xD0\x5E\x86\x76\x8C"
          "\x31\x3E\x3B\xFE\xF7\x51\x19\x37\xDC\xF7\x2C\x3E" },
        0,
        FALSE
    },
    {
        ENCTYPE_DES3_CBC_SHA1,
        "potatoe",
        { KV5M_DATA, 19, "WHITEHOUSE.GOVdanny" },
        { KV5M_DATA, 0, NULL },
        { KV5M_DATA, 24, "\xDF\xCD\x23\x3D\xD0\xA4\x32\x04\xEA\x6D\xC4\x37"
          "\xFB\x15\xE0\x61\xB0\x29\x79\xC1\xF7\x4F\x37\x7A" },
        0,
        FALSE
    },
    {
        ENCTYPE_DES3_CBC_SHA1,
        "penny",
        { KV5M_DATA, 19, "EXAMPLE.COMbuckaroo" },
        { KV5M_DATA, 0, NULL },
        { KV5M_DATA, 24, "\x6D\x2F\xCD\xF2\xD6\xFB\xBC\x3D\xDC\xAD\xB5\xDA"
          "\x57\x10\xA2\x34\x89\xB0\xD3\xB6\x9D\x5D\x9D\x4A" },
        0,
        FALSE
    },
    {
        ENCTYPE_DES3_CBC_SHA1,
        "\xC3\x9F",
        { KV5M_DATA, 23, "ATHENA.MIT.EDUJuri\xC5\xA1\x69\xC4\x87" },
        { KV5M_DATA, 0, NULL },
        { KV5M_DATA, 24, "\x16\xD5\xA4\x0E\x1C\xE3\xBA\xCB\x61\xB9\xDC\xE0"
          "\x04\x70\x32\x4C\x83\x19\x73\xA7\xB9\x52\xFE\xB0" },
        0,
        FALSE
    },
    {
        ENCTYPE_DES3_CBC_SHA1,
        "\xF0\x9D\x84\x9E",
        { KV5M_DATA, 18, "EXAMPLE.COMpianist" },
        { KV5M_DATA, 0, NULL },
        { KV5M_DATA, 24, "\x85\x76\x37\x26\x58\x5D\xBC\x1C\xCE\x6E\xC4\x3E"
          "\x1F\x75\x1F\x07\xF1\xC4\xCB\xB0\x98\xF4\x0B\x19" },
        0,
        FALSE
    },

    /* Test vectors from RFC 3962 appendix B. */
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\0\1" },
        { KV5M_DATA, 16,
          "\x42\x26\x3C\x6E\x89\xF4\xFC\x28\xB8\xDF\x68\xEE\x09\x79\x9F\x15" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\0\1" },
        { KV5M_DATA, 32,
          "\xFE\x69\x7B\x52\xBC\x0D\x3C\xE1\x44\x32\xBA\x03\x6A\x92\xE6\x5B"
          "\xBB\x52\x28\x09\x90\xA2\xFA\x27\x88\x39\x98\xD7\x2A\xF3\x01\x61" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\0\2" },
        { KV5M_DATA, 16,
          "\xC6\x51\xBF\x29\xE2\x30\x0A\xC2\x7F\xA4\x69\xD6\x93\xBD\xDA\x13" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\0\2" },
        { KV5M_DATA, 32,
          "\xA2\xE1\x6D\x16\xB3\x60\x69\xC1\x35\xD5\xE9\xD2\xE2\x5F\x89\x61"
          "\x02\x68\x56\x18\xB9\x59\x14\xB4\x67\xC6\x76\x22\x22\x58\x24\xFF" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 16,
          "\x4C\x01\xCD\x46\xD6\x32\xD0\x1E\x6D\xBE\x23\x0A\x01\xED\x64\x2A" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 32,
          "\x55\xA6\xAC\x74\x0A\xD1\x7B\x48\x46\x94\x10\x51\xE1\xE8\xB0\xA7"
          "\x54\x8D\x93\xB0\xAB\x30\xA8\xBC\x3F\xF1\x62\x80\x38\x2B\x8C\x2A" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        "password",
        { KV5M_DATA, 8, "\x12\x34\x56\x78\x78\x56\x34\x12" },
        { KV5M_DATA, 4, "\0\0\0\5" },
        { KV5M_DATA, 16,
          "\xE9\xB2\x3D\x52\x27\x37\x47\xDD\x5C\x35\xCB\x55\xBE\x61\x9D\x8E" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        "password",
        { KV5M_DATA, 8, "\x12\x34\x56\x78\x78\x56\x34\x12" },
        { KV5M_DATA, 4, "\0\0\0\5" },
        { KV5M_DATA, 32,
          "\x97\xA4\xE7\x86\xBE\x20\xD8\x1A\x38\x2D\x5E\xBC\x96\xD5\x90\x9C"
          "\xAB\xCD\xAD\xC8\x7C\xA4\x8F\x57\x45\x04\x15\x9F\x16\xC3\x6E\x31" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        { KV5M_DATA, 29, "pass phrase equals block size" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 16,
          "\x59\xD1\xBB\x78\x9A\x82\x8B\x1A\xA5\x4E\xF9\xC2\x88\x3F\x69\xED" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        { KV5M_DATA, 29, "pass phrase equals block size" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 32,
          "\x89\xAD\xEE\x36\x08\xDB\x8B\xC7\x1F\x1B\xFB\xFE\x45\x94\x86\xB0"
          "\x56\x18\xB7\x0C\xBA\xE2\x20\x92\x53\x4E\x56\xC5\x53\xBA\x4B\x34" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        { KV5M_DATA, 30, "pass phrase exceeds block size" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 16,
          "\xCB\x80\x05\xDC\x5F\x90\x17\x9A\x7F\x02\x10\x4C\x00\x18\x75\x1D" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        { KV5M_DATA, 30, "pass phrase exceeds block size" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 32,
          "\xD7\x8C\x5C\x9C\xB8\x72\xA8\xC9\xDA\xD4\x69\x7F\x0B\xB5\xB2\xD2"
          "\x14\x96\xC8\x2B\xEB\x2C\xAE\xDA\x21\x12\xFC\xEE\xA0\x57\x40\x1B" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        "\xF0\x9D\x84\x9E",
        { KV5M_DATA, 18, "EXAMPLE.COMpianist" },
        { KV5M_DATA, 4, "\0\0\0\x32" }, /* 50 */
        { KV5M_DATA, 16,
          "\xF1\x49\xC1\xF2\xE1\x54\xA7\x34\x52\xD4\x3E\x7F\xE6\x2A\x56\xE5" },
        0,
        TRUE
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        "\xF0\x9D\x84\x9E",
        { KV5M_DATA, 18, "EXAMPLE.COMpianist" },
        { KV5M_DATA, 4, "\0\0\0\x32" }, /* 50 */
        { KV5M_DATA, 32,
          "\x4B\x6D\x98\x39\xF8\x44\x06\xDF\x1F\x09\xCC\x16\x6D\xB4\xB8\x3C"
          "\x57\x18\x48\xB7\x84\xA3\xD6\xBD\xC3\x46\x58\x9A\x3E\x39\x3F\x9E" },
        0,
        TRUE
    },
    /* Check for KRB5_ERR_BAD_S2K_PARAMS return when weak iteration counts are
     * forbidden. */
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        "\xF0\x9D\x84\x9E",
        { KV5M_DATA, 18, "EXAMPLE.COMpianist" },
        { KV5M_DATA, 4, "\0\0\0\x32" }, /* 50 */
        { KV5M_DATA, 32,
          "\x4B\x6D\x98\x39\xF8\x44\x06\xDF\x1F\x09\xCC\x16\x6D\xB4\xB8\x3C"
          "\x57\x18\x48\xB7\x84\xA3\xD6\xBD\xC3\x46\x58\x9A\x3E\x39\x3F\x9E" },
        KRB5_ERR_BAD_S2K_PARAMS,
        FALSE
    },

    /* The same inputs applied to Camellia enctypes. */
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\0\1" },
        { KV5M_DATA, 16,
          "\x57\xD0\x29\x72\x98\xFF\xD9\xD3\x5D\xE5\xA4\x7F\xB4\xBD\xE2\x4B" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\0\1" },
        { KV5M_DATA, 32,
          "\xB9\xD6\x82\x8B\x20\x56\xB7\xBE\x65\x6D\x88\xA1\x23\xB1\xFA\xC6"
          "\x82\x14\xAC\x2B\x72\x7E\xCF\x5F\x69\xAF\xE0\xC4\xDF\x2A\x6D\x2C" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\0\2" },
        { KV5M_DATA, 16,
          "\x73\xF1\xB5\x3A\xA0\xF3\x10\xF9\x3B\x1D\xE8\xCC\xAA\x0C\xB1\x52" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\0\2" },
        { KV5M_DATA, 32,
          "\x83\xFC\x58\x66\xE5\xF8\xF4\xC6\xF3\x86\x63\xC6\x5C\x87\x54\x9F"
          "\x34\x2B\xC4\x7E\xD3\x94\xDC\x9D\x3C\xD4\xD1\x63\xAD\xE3\x75\xE3" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 16,
          "\x8E\x57\x11\x45\x45\x28\x55\x57\x5F\xD9\x16\xE7\xB0\x44\x87\xAA" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        "password",
        { KV5M_DATA, 21, "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 32,
          "\x77\xF4\x21\xA6\xF2\x5E\x13\x83\x95\xE8\x37\xE5\xD8\x5D\x38\x5B"
          "\x4C\x1B\xFD\x77\x2E\x11\x2C\xD9\x20\x8C\xE7\x2A\x53\x0B\x15\xE6" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        "password",
        { KV5M_DATA, 8, "\x12\x34\x56\x78\x78\x56\x34\x12" },
        { KV5M_DATA, 4, "\0\0\0\5" },
        { KV5M_DATA, 16,
          "\x00\x49\x8F\xD9\x16\xBF\xC1\xC2\xB1\x03\x1C\x17\x08\x01\xB3\x81" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        "password",
        { KV5M_DATA, 8, "\x12\x34\x56\x78\x78\x56\x34\x12" },
        { KV5M_DATA, 4, "\0\0\0\5" },
        { KV5M_DATA, 32,
          "\x11\x08\x3A\x00\xBD\xFE\x6A\x41\xB2\xF1\x97\x16\xD6\x20\x2F\x0A"
          "\xFA\x94\x28\x9A\xFE\x8B\x27\xA0\x49\xBD\x28\xB1\xD7\x6C\x38\x9A" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        { KV5M_DATA, 29, "pass phrase equals block size" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 16,
          "\x8B\xF6\xC3\xEF\x70\x9B\x98\x1D\xBB\x58\x5D\x08\x68\x43\xBE\x05" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        { KV5M_DATA, 29, "pass phrase equals block size" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 32,
          "\x11\x9F\xE2\xA1\xCB\x0B\x1B\xE0\x10\xB9\x06\x7A\x73\xDB\x63\xED"
          "\x46\x65\xB4\xE5\x3A\x98\xD1\x78\x03\x5D\xCF\xE8\x43\xA6\xB9\xB0" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        { KV5M_DATA, 30, "pass phrase exceeds block size" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 16,
          "\x57\x52\xAC\x8D\x6A\xD1\xCC\xFE\x84\x30\xB3\x12\x87\x1C\x2F\x74" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        { KV5M_DATA, 30, "pass phrase exceeds block size" },
        { KV5M_DATA, 4, "\0\0\x04\xB0" }, /* 1200 */
        { KV5M_DATA, 32,
          "\x61\x4D\x5D\xFC\x0B\xA6\xD3\x90\xB4\x12\xB8\x9A\xE4\xD5\xB0\x88"
          "\xB6\x12\xB3\x16\x51\x09\x94\x67\x9D\xDB\x43\x83\xC7\x12\x6D\xDF" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA128_CTS_CMAC,
        "\xf0\x9d\x84\x9e",
        { KV5M_DATA, 18, "EXAMPLE.COMpianist" },
        { KV5M_DATA, 4, "\0\0\0\x32" }, /* 50 */
        { KV5M_DATA, 16,
          "\xCC\x75\xC7\xFD\x26\x0F\x1C\x16\x58\x01\x1F\xCC\x0D\x56\x06\x16" },
        0,
        TRUE
    },
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        "\xf0\x9d\x84\x9e",
        { KV5M_DATA, 18, "EXAMPLE.COMpianist" },
        { KV5M_DATA, 4, "\0\0\0\x32" }, /* 50 */
        { KV5M_DATA, 32,
          "\x16\x3B\x76\x8C\x6D\xB1\x48\xB4\xEE\xC7\x16\x3D\xF5\xAE\xD7\x0E"
          "\x20\x6B\x68\xCE\xC0\x78\xBC\x06\x9E\xD6\x8A\x7E\xD3\x6B\x1E\xCC" },
        0,
        TRUE
    },
    /* Check for KRB5_ERR_BAD_S2K_PARAMS return when weak iteration counts are
     * forbidden. */
    {
        ENCTYPE_CAMELLIA256_CTS_CMAC,
        "\xf0\x9d\x84\x9e",
        { KV5M_DATA, 18, "EXAMPLE.COMpianist" },
        { KV5M_DATA, 4, "\0\0\0\x32" }, /* 50 */
        { KV5M_DATA, 32,
          "\x16\x3B\x76\x8C\x6D\xB1\x48\xB4\xEE\xC7\x16\x3D\xF5\xAE\xD7\x0E"
          "\x20\x6B\x68\xCE\xC0\x78\xBC\x06\x9E\xD6\x8A\x7E\xD3\x6B\x1E\xCC" },
        KRB5_ERR_BAD_S2K_PARAMS,
        FALSE
    },

    {
        ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        "password",
        { KV5M_DATA, 37,
          "\x10\xDF\x9D\xD7\x83\xE5\xBC\x8A\xCE\xA1\x73\x0E\x74\x35\x5F\x61"
          "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\x00\x00\x80\x00" },
        { KV5M_DATA, 16,
          "\x08\x9B\xCA\x48\xB1\x05\xEA\x6E\xA7\x7C\xA5\xD2\xF3\x9D\xC5\xE7" },
        0,
        FALSE
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA384_192,
        "password",
        { KV5M_DATA, 37,
          "\x10\xDF\x9D\xD7\x83\xE5\xBC\x8A\xCE\xA1\x73\x0E\x74\x35\x5F\x61"
          "ATHENA.MIT.EDUraeburn" },
        { KV5M_DATA, 4, "\x00\x00\x80\x00" },
        { KV5M_DATA, 32,
          "\x45\xBD\x80\x6D\xBF\x6A\x83\x3A\x9C\xFF\xC1\xC9\x45\x89\xA2\x22"
          "\x36\x7A\x79\xBC\x21\xC4\x13\x71\x89\x06\xE9\xF5\x78\xA7\x84\x67" },
        0,
        FALSE
    },
};

static void
printhex(const char *head, void *data, size_t len)
{
    size_t i;

    printf("%s", head);
    for (i = 0; i < len; i++) {
        printf("%02X", ((unsigned char*)data)[i]);
        if (i % 16 == 15 && i + 1 < len)
            printf("\n%*s", (int)strlen(head), "");
        else if (i + 1 < len)
            printf(" ");
    }
    printf("\n");
}

extern int k5_allow_weak_pbkdf2iter;

int
main(int argc, char **argv)
{
    krb5_context context = NULL;
    krb5_data string;
    krb5_error_code ret;
    krb5_keyblock *keyblock;
    size_t i;
    struct test *test;
    krb5_boolean verbose = FALSE;
    int status = 0;

    if (argc >= 2 && strcmp(argv[1], "-v") == 0)
        verbose = TRUE;
    for (i = 0; i < sizeof(test_cases) / sizeof(*test_cases); i++) {
        test = &test_cases[i];
        string = string2data(test->string);
        ret = krb5_init_keyblock(context, test->enctype, 0, &keyblock);
        assert(!ret);
        k5_allow_weak_pbkdf2iter = test->allow_weak;
        ret = krb5_c_string_to_key_with_params(context, test->enctype,
                                               &string, &test->salt,
                                               &test->params, keyblock);
        if (ret != test->expected_err) {
            com_err(argv[0], ret, "in krb5_c_string_to_key_with_params");
            exit(1);
        }
        if (verbose) {
            char buf[64];
            krb5_enctype_to_name(test->enctype, FALSE, buf, sizeof(buf));
            printf("\nTest %d:\n", (int)i);
            printf("Enctype: %s\n", buf);
            printf("String: %s\n", test->string);
            printhex("Salt: ", test->salt.data, test->salt.length);
            printhex("Params: ", test->params.data, test->params.length);
            if (test->expected_err == 0)
                printhex("Key: ", keyblock->contents, keyblock->length);
            else
                printf("Expected error: %d\n", (int)test->expected_err);
        }
        if (test->expected_err != 0) {
            krb5_free_keyblock(context, keyblock);
            continue;
        }
        assert(keyblock->length == test->expected_key.length);
        if (memcmp(keyblock->contents, test->expected_key.data,
                   keyblock->length) != 0) {
            printf("str2key test %d failed\n", (int)i);
            status = 1;
            if (!verbose)
                break;
        }
        krb5_free_keyblock(context, keyblock);
    }
    return status;
}
