/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_parse_host_string.c - k5_parse_host_string() unit tests */
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
#include "k5-cmocka.h"

/* Call k5_parse_host_string() and check the result against the expected code,
 * hostname, and port. */
static void
call_k5_parse_host_string(const char *host, int default_port,
                          krb5_error_code e_code, const char *e_host,
                          int e_port)
{
    krb5_error_code code;
    char *host_out = NULL;
    int port_out = -1;

    code = k5_parse_host_string(host, default_port, &host_out, &port_out);

    assert_int_equal(code, e_code);

    /* Only check the port if the function was expected to be successful. */
    if (!e_code)
        assert_int_equal(port_out, e_port);

    /* If the expected code is a failure then host_out should be NULL. */
    if (e_code != 0 || e_host == NULL)
        assert_null(host_out);
    else
        assert_string_equal(e_host, host_out);

    free(host_out);
}

/* k5_parse_host_string() tests */

static void
test_named_host_only(void **state)
{
    call_k5_parse_host_string("test.example", 50, 0, "test.example", 50);
}

static void
test_named_host_w_port(void **state)
{
    call_k5_parse_host_string("test.example:75", 0, 0, "test.example", 75);
}

static void
test_ipv4_only(void **state)
{
    call_k5_parse_host_string("192.168.1.1", 100, 0, "192.168.1.1", 100);
}

static void
test_ipv4_w_port(void **state)
{
    call_k5_parse_host_string("192.168.1.1:150", 0, 0, "192.168.1.1", 150);
}

static void
test_ipv6_only(void **state)
{
    call_k5_parse_host_string("[BEEF:CAFE:FEED:FACE:DEAD:BEEF:DEAF:BABE]", 200,
                              0, "BEEF:CAFE:FEED:FACE:DEAD:BEEF:DEAF:BABE",
                              200);
}

static void
test_ipv6_w_port(void **state)
{
    call_k5_parse_host_string("[BEEF:CAFE:FEED:FACE:DEAD:BEEF:DEAF:BABE]:250",
                              0, 0, "BEEF:CAFE:FEED:FACE:DEAD:BEEF:DEAF:BABE",
                              250);
}

static void
test_ipv6_w_zone(void **state)
{
    call_k5_parse_host_string("[BEEF:CAFE:FEED:FACE:DEAD:BEEF:DEAF:BABE%eth0]",
                              275, 0,
                              "BEEF:CAFE:FEED:FACE:DEAD:BEEF:DEAF:BABE%eth0",
                              275);
}

static void
test_invalid_ipv6(void **state)
{
    call_k5_parse_host_string("BEEF:CAFE:FEED:FACE:DEAD:BEEF:DEAF:BABE", 1,
                              EINVAL, NULL, 0);
}

static void
test_no_host_port(void **state)
{
    call_k5_parse_host_string(":300", 0, EINVAL, NULL, 300);
}

static void
test_port_only(void **state)
{
    call_k5_parse_host_string("350", 0, 0, NULL, 350);
}

static void
test_null_host(void **state)
{
    call_k5_parse_host_string(NULL, 400, EINVAL, NULL, 400);
}

static void
test_empty_host(void **state)
{
    call_k5_parse_host_string("", 450, EINVAL, NULL, 450);
}

static void
test_port_out_of_range(void **state)
{
    call_k5_parse_host_string("70000", 1, EINVAL, NULL, 0);
}

static void
test_port_invalid_characters(void **state)
{
    call_k5_parse_host_string("test.example:F101", 1, EINVAL, NULL, 0);
}

static void
test_invalid_default_port(void **state)
{
    call_k5_parse_host_string("test.example", 70000, EINVAL, NULL, 0);
}

/* k5_is_string_numeric() tests */

static void
test_numeric_single_digit(void **state)
{
    assert_true(k5_is_string_numeric("0"));
}

static void
test_numeric_all_digits(void **state)
{
    assert_true(k5_is_string_numeric("0123456789"));
}

static void
test_numeric_alpha(void **state)
{
    assert_false(k5_is_string_numeric("012345F6789"));
}

static void
test_numeric_period(void **state)
{
    assert_false(k5_is_string_numeric("123.456"));
}

static void
test_numeric_negative(void **state)
{
    assert_false(k5_is_string_numeric("-123"));
}

static void
test_numeric_empty(void **state)
{
    assert_false(k5_is_string_numeric(""));
}

static void
test_numeric_whitespace(void **state)
{
    assert_false(k5_is_string_numeric("123 456"));
}

int
main(void)
{
    int ret;

    const struct CMUnitTest k5_parse_host_string_tests[] = {
        cmocka_unit_test(test_named_host_only),
        cmocka_unit_test(test_named_host_w_port),
        cmocka_unit_test(test_ipv4_only),
        cmocka_unit_test(test_ipv4_w_port),
        cmocka_unit_test(test_ipv6_only),
        cmocka_unit_test(test_ipv6_w_port),
        cmocka_unit_test(test_ipv6_w_zone),
        cmocka_unit_test(test_invalid_ipv6),
        cmocka_unit_test(test_no_host_port),
        cmocka_unit_test(test_port_only),
        cmocka_unit_test(test_null_host),
        cmocka_unit_test(test_empty_host),
        cmocka_unit_test(test_port_out_of_range),
        cmocka_unit_test(test_port_invalid_characters),
        cmocka_unit_test(test_invalid_default_port)
    };

    const struct CMUnitTest k5_is_string_numeric_tests[] = {
        cmocka_unit_test(test_numeric_single_digit),
        cmocka_unit_test(test_numeric_all_digits),
        cmocka_unit_test(test_numeric_alpha),
        cmocka_unit_test(test_numeric_period),
        cmocka_unit_test(test_numeric_negative),
        cmocka_unit_test(test_numeric_empty),
        cmocka_unit_test(test_numeric_whitespace)
    };

    ret = cmocka_run_group_tests_name("k5_parse_host_string",
                                      k5_parse_host_string_tests, NULL, NULL);
    ret += cmocka_run_group_tests_name("k5_is_string_numeric",
                                       k5_is_string_numeric_tests, NULL, NULL);

    return ret;
}
