/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_prf.c - PRF test cases */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"

struct test {
    krb5_enctype enctype;
    krb5_data keybits;
    krb5_data prf_input;
    krb5_data expected;
} tests[] = {
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 16,
          "\xAE\x27\x2E\x7C\xDE\xC8\x6A\xC5\x13\x8C\xDB\x19\x6D\x8E\x29\x7D" },
        { KV5M_DATA, 2, "\x01\x61" },
        { KV5M_DATA, 16,
          "\x77\xB3\x9A\x37\xA8\x68\x92\x0F\x2A\x51\xF9\xDD\x15\x0C\x57\x17" }
    },
    {
        ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 16,
          "\x67\xAB\x1C\xFE\xF3\x5E\x4C\x27\xFF\xDE\xAC\x60\x38\x5A\x3E\x9C" },
        { KV5M_DATA, 2, "\x01\x62" },
        { KV5M_DATA, 16,
          "\xE0\x6C\x0D\xD3\x1F\xF0\x20\x91\x99\x4F\x2E\xF5\x17\x8B\xFE\x3D" }
    },

    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 32,
          "\xC0\x1F\x15\x72\x11\xF7\xB7\x7E\xAA\xF4\x57\xC3\xE1\x56\x69\x01"
          "\x27\xEE\x12\x7D\x81\x0B\xA6\x39\x2E\x97\xBA\xA2\x43\xEB\x06\x16" },
        { KV5M_DATA, 2, "\x01\x61" },
        { KV5M_DATA, 16,
          "\xB2\x62\x8C\x78\x8E\x2E\x9C\x4A\x9B\xB4\x64\x46\x78\xC2\x9F\x2F" }
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 32,
          "\xC0\x1F\x15\x72\x11\xF7\xB7\x7E\xAA\xF4\x57\xC3\xE1\x56\x69\x01"
          "\x27\xEE\x12\x7D\x81\x0B\xA6\x39\x2E\x97\xBA\xA2\x43\xEB\x06\x16" },
        { KV5M_DATA, 2, "\x02\x61" },
        { KV5M_DATA, 16,
          "\xB4\x06\x37\x33\x50\xCE\xE8\xA6\x12\x6F\x4A\x9B\x65\xA0\xCD\x21" }
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 32,
          "\x9D\x52\x0D\x2D\x98\x0A\xA7\xCB\x6B\x69\x36\x82\xB6\x2D\xA2\x58"
          "\xB3\x33\x86\x79\x51\x64\x2C\xE6\x47\xAE\x62\xB1\xE5\xE0\xB5\xE9" },
        { KV5M_DATA, 2, "\x01\x62" },
        { KV5M_DATA, 16,
          "\xFF\x0E\x28\x9E\xA7\x56\xC0\x55\x9A\x0E\x91\x18\x56\x96\x1A\x49" }
    },
    {
        ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        { KV5M_DATA, 32,
          "\x9D\x52\x0D\x2D\x98\x0A\xA7\xCB\x6B\x69\x36\x82\xB6\x2D\xA2\x58"
          "\xB3\x33\x86\x79\x51\x64\x2C\xE6\x47\xAE\x62\xB1\xE5\xE0\xB5\xE9" },
        { KV5M_DATA, 2, "\x02\x62" },
        { KV5M_DATA, 16,
          "\x0D\x67\x4D\xD0\xF9\xA6\x80\x65\x25\xA4\xD9\x2E\x82\x8B\xD1\x5A" }
    },

    {
        ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        { KV5M_DATA, 16,
          "\x37\x05\xD9\x60\x80\xC1\x77\x28\xA0\xE8\x00\xEA\xB6\xE0\xD2\x3C" },
        { KV5M_DATA, 4, "test" },
        { KV5M_DATA, 32,
          "\x9D\x18\x86\x16\xF6\x38\x52\xFE\x86\x91\x5B\xB8\x40\xB4\xA8\x86"
          "\xFF\x3E\x6B\xB0\xF8\x19\xB4\x9B\x89\x33\x93\xD3\x93\x85\x42\x95" }
    },

    {
        ENCTYPE_AES256_CTS_HMAC_SHA384_192,
        { KV5M_DATA, 32,
          "\x6D\x40\x4D\x37\xFA\xF7\x9F\x9D\xF0\xD3\x35\x68\xD3\x20\x66\x98"
          "\x00\xEB\x48\x36\x47\x2E\xA8\xA0\x26\xD1\x6B\x71\x82\x46\x0C\x52" },
        { KV5M_DATA, 4, "test" },
        { KV5M_DATA, 48,
          "\x98\x01\xF6\x9A\x36\x8C\x2B\xF6\x75\xE5\x95\x21\xE1\x77\xD9\xA0"
          "\x7F\x67\xEF\xE1\xCF\xDE\x8D\x3C\x8D\x6F\x6A\x02\x56\xE3\xB1\x7D"
          "\xB3\xC1\xB6\x2A\xD1\xB8\x55\x33\x60\xD1\x73\x67\xEB\x15\x14\xD2" }
    },
};

int
main()
{
    krb5_error_code ret;
    krb5_data output;
    krb5_keyblock kb;
    size_t i, prfsz;
    const struct test *test;

    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        test = &tests[i];
        kb.magic = KV5M_KEYBLOCK;
        kb.enctype = test->enctype;
        kb.length = test->keybits.length;
        kb.contents = (uint8_t *)test->keybits.data;

        ret = krb5_c_prf_length(NULL, test->enctype, &prfsz);
        assert(!ret);
        ret = alloc_data(&output, prfsz);
        assert(!ret);
        ret = krb5_c_prf(NULL, &kb, &tests[i].prf_input, &output);
        assert(!ret);

        if (!data_eq(output, tests[i].expected)) {
            printf("Test %d failed\n", (int)i);
            exit(1);
        }
        free(output.data);
    }

    return 0;
}
