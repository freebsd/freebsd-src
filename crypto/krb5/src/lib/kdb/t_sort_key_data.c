/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kdb/t_sort_key_data.c - krb5_dbe_sort_key_data() unit tests */
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

#include "k5-cmocka.h"
#include "kdb.h"

#define KEY(kvno) {                                             \
        1, kvno, { ENCTYPE_AES128_CTS_HMAC_SHA1_96, 0 },        \
        { 16, 0 },                                              \
        { (uint8_t *)("\xDC\xEE\xB7\x0B\x3D\xE7\x65\x62"        \
                      "\xE6\x89\x22\x6C\x76\x42\x91\x48"),      \
                NULL }                                          \
    }

static void
assert_sorted(krb5_key_data *keys, int num_keys)
{
    int i;

    for (i = 1; i < num_keys; i++)
        assert_true(keys[i].key_data_kvno <= keys[i - 1].key_data_kvno);
}

static void
test_pre_sorted(void **state)
{
    krb5_key_data keys[] = { KEY(5), KEY(5), KEY(4), KEY(3), KEY(3), KEY(2),
                             KEY(2), KEY(1) };
    int n_keys = sizeof(keys)/sizeof(keys[0]);

    krb5_dbe_sort_key_data(keys, n_keys);
    assert_sorted(keys, n_keys);
}

static void
test_reverse_sorted(void **state)
{
    krb5_key_data keys[] = { KEY(1), KEY(2), KEY(2), KEY(3), KEY(3), KEY(3),
                             KEY(4), KEY(5) };
    int n_keys = sizeof(keys)/sizeof(keys[0]);

    krb5_dbe_sort_key_data(keys, n_keys);
    assert_sorted(keys, n_keys);
}

static void
test_random_order(void **state)
{
    krb5_key_data keys[] = { KEY(1), KEY(4), KEY(1), KEY(3), KEY(4), KEY(3),
                             KEY(5), KEY(2) };
    int n_keys = sizeof(keys)/sizeof(keys[0]);

    krb5_dbe_sort_key_data(keys, n_keys);
    assert_sorted(keys, n_keys);
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_pre_sorted),
        cmocka_unit_test(test_reverse_sorted),
        cmocka_unit_test(test_random_order)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
