/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <atf-c++.hpp>
#include <stdarg.h>
#include <stdio.h>

#include <libutil++.hh>

ATF_TEST_CASE_WITHOUT_HEAD(basic);
ATF_TEST_CASE_BODY(basic)
{
	ATF_REQUIRE_EQ("foo", freebsd::stringf("foo"));
	ATF_REQUIRE_EQ("bar", freebsd::stringf("%s", "bar"));
	ATF_REQUIRE_EQ("42", freebsd::stringf("%u", 42));
	ATF_REQUIRE_EQ("0xdeadbeef", freebsd::stringf("%#x", 0xdeadbeef));
	ATF_REQUIRE_EQ("", freebsd::stringf(""));
	ATF_REQUIRE_EQ("this is a test", freebsd::stringf("this %s test",
	    "is a"));
}

static std::string
stringv(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	std::string str = freebsd::stringf(fmt, ap);
	va_end(ap);
	return (str);
}

ATF_TEST_CASE_WITHOUT_HEAD(va_list);
ATF_TEST_CASE_BODY(va_list)
{
	ATF_REQUIRE_EQ("foo", stringv("foo"));
	ATF_REQUIRE_EQ("bar", stringv("%s", "bar"));
	ATF_REQUIRE_EQ("42", stringv("%u", 42));
	ATF_REQUIRE_EQ("0xdeadbeef", stringv("%#x", 0xdeadbeef));
	ATF_REQUIRE_EQ("", stringv(""));
	ATF_REQUIRE_EQ("this is a test", stringv("this %s test", "is a"));
}

ATF_INIT_TEST_CASES(tcs)
{
	ATF_ADD_TEST_CASE(tcs, basic);
	ATF_ADD_TEST_CASE(tcs, va_list);
}
