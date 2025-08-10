/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/fuzz_des.c - fuzzing harness for DES functions */
/*
 * Copyright (C) 2024 by Arjun. All rights reserved.
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

#include "autoconf.h"
#include <k5-int.h>
#include <des_int.h>

#include <f_cbc.c>

#define kMinInputLength 32
#define kMaxInputLength 128

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

uint8_t default_ivec[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };

static void
fuzz_des(uint8_t *input, mit_des_key_schedule sched)
{
    uint8_t encrypt[8], decrypt[8];

    mit_des_cbc_encrypt((const mit_des_cblock *)input,
                        (mit_des_cblock *)encrypt, 8,
                        sched, default_ivec, MIT_DES_ENCRYPT);

    mit_des_cbc_encrypt((const mit_des_cblock *)encrypt,
                        (mit_des_cblock *)decrypt, 8,
                        sched, default_ivec, MIT_DES_DECRYPT);

    if (memcmp(input, decrypt, 8) != 0)
        abort();
}

static void
fuzz_decrypt(uint8_t *input, mit_des_key_schedule sched)
{
    uint8_t output[8];

    mit_des_cbc_encrypt((const mit_des_cblock *)input,
                        (mit_des_cblock *)output, 8,
                        sched, default_ivec, MIT_DES_DECRYPT);
}

static void
fuzz_cksum(uint8_t *input, mit_des_key_schedule sched)
{
    uint8_t output[8];

    mit_des_cbc_cksum(input, output, 8, sched, default_ivec);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    krb5_error_code ret;
    mit_des_key_schedule sched;
    uint8_t *data_in, input[8];

    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    memcpy(input, data, 8);
    ret = mit_des_key_sched(input, sched);
    if (ret)
        return 0;

    memcpy(input, data + 8, 8);
    fuzz_des(input, sched);

    memcpy(input, data + 16, 8);
    fuzz_decrypt(input, sched);

    data_in = k5memdup(data + 24, size - 24, &ret);
    if (ret)
        return 0;

    fuzz_cksum(data_in, sched);
    free(data_in);

    return 0;
}
