/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 */

#include <limits.h>
#include <atf-c.h>

static int
#include "parse_integer_func.c"

ATF_TC_WITHOUT_HEAD(integers);
ATF_TC_BODY(integers, tc)
{
	ATF_REQUIRE_EQ(parse_integer("0"), 0);
	ATF_REQUIRE_EQ(parse_integer("10"), 10);
	ATF_REQUIRE_EQ(parse_integer("10001"), 10001);
	ATF_REQUIRE_EQ(parse_integer("0b101"), 0b101);
	ATF_REQUIRE_EQ(parse_integer("0x10"), 0x10);
	ATF_REQUIRE_EQ(parse_integer("020"), 020);
	ATF_REQUIRE_EQ(parse_integer("090"), -1);
	/* This test assumes some value for INT_MAX */
	ATF_REQUIRE_EQ(parse_integer("1111111111111111111111111111"), -1);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, integers);
	return (atf_no_error());
}
