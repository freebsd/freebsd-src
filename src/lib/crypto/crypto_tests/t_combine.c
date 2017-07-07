/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_combine.c - krb5int_c_combine_keys tests */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
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

unsigned char des_key1[] = "\x04\x86\xCD\x97\x61\xDF\xD6\x29";
unsigned char des_key2[] = "\x1A\x54\x9B\x7F\xDC\x20\x83\x0E";
unsigned char des_result[] = "\xC2\x13\x01\x52\x89\x26\xC4\xF7";

unsigned char des3_key1[] = "\x10\xB6\x75\xD5\x5B\xD9\x6E\x73"
    "\xFD\x54\xB3\x3D\x37\x52\xC1\x2A\xF7\x43\x91\xFE\x1C\x02\x37\x13";
unsigned char des3_key2[] = "\xC8\xDA\x3E\xA7\xB6\x64\xAE\x7A"
    "\xB5\x70\x2A\x29\xB3\xBF\x9B\xA8\x46\x7C\x5B\xA8\x8A\x46\x70\x10";
unsigned char des3_result[] = "\x2F\x79\x97\x3E\x3E\xA4\x73\x1A"
    "\xB9\x3D\xEF\x5E\x7C\x29\xFB\x2A\x68\x86\x1F\xC1\x85\x0E\x79\x92";

int
main(int argc, char **argv)
{
    krb5_keyblock kb1, kb2, result;

    kb1.enctype = ENCTYPE_DES_CBC_CRC;
    kb1.contents = des_key1;
    kb1.length = 8;
    kb2.enctype = ENCTYPE_DES_CBC_CRC;
    kb2.contents = des_key2;
    kb2.length = 8;
    memset(&result, 0, sizeof(result));
    if (krb5int_c_combine_keys(NULL, &kb1, &kb2, &result) != 0)
        abort();
    if (result.enctype != ENCTYPE_DES_CBC_CRC || result.length != 8 ||
        memcmp(result.contents, des_result, 8) != 0)
        abort();
    krb5_free_keyblock_contents(NULL, &result);

    kb1.enctype = ENCTYPE_DES3_CBC_SHA1;
    kb1.contents = des3_key1;
    kb1.length = 24;
    kb2.enctype = ENCTYPE_DES3_CBC_SHA1;
    kb2.contents = des3_key2;
    kb2.length = 24;
    memset(&result, 0, sizeof(result));
    if (krb5int_c_combine_keys(NULL, &kb1, &kb2, &result) != 0)
        abort();
    if (result.enctype != ENCTYPE_DES3_CBC_SHA1 || result.length != 24 ||
        memcmp(result.contents, des3_result, 24) != 0)
        abort();
    krb5_free_keyblock_contents(NULL, &result);

    return 0;
}
