/* $NetBSD: t_strptime.c,v 1.1 2011/01/13 00:14:10 pgoyette Exp $ */

/*-
 * Copyright (c) 1998, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__COPYRIGHT("@(#) Copyright (c) 2008\
 The NetBSD Foundation, inc. All rights reserved.");
__RCSID("$NetBSD: t_strptime.c,v 1.1 2011/01/13 00:14:10 pgoyette Exp $");

#include <time.h>

#include <atf-c.h>

static void
h_pass(const char *buf, const char *fmt, int len,
    int tm_sec, int tm_min, int tm_hour, int tm_mday,
    int tm_mon, int tm_year, int tm_wday, int tm_yday)
{
	struct tm tm = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, NULL };
	const char *ret, *exp;

	exp = buf + len;
	ret = strptime(buf, fmt, &tm);

	ATF_REQUIRE_MSG(ret == exp,
	    "strptime(\"%s\", \"%s\", tm): incorrect return code: "
	    "expected: %p, got: %p", buf, fmt, exp, ret);

#define H_REQUIRE_FIELD(field)						\
		ATF_REQUIRE_MSG(tm.field == field,			\
		    "strptime(\"%s\", \"%s\", tm): incorrect %s: "	\
		    "expected: %d, but got: %d", buf, fmt,		\
		    ___STRING(field), field, tm.field)

	H_REQUIRE_FIELD(tm_sec);
	H_REQUIRE_FIELD(tm_min);
	H_REQUIRE_FIELD(tm_hour);
	H_REQUIRE_FIELD(tm_mday);
	H_REQUIRE_FIELD(tm_mon);
	H_REQUIRE_FIELD(tm_year);
	H_REQUIRE_FIELD(tm_wday);
	H_REQUIRE_FIELD(tm_yday);

#undef H_REQUIRE_FIELD
}

static void
h_fail(const char *buf, const char *fmt)
{
	struct tm tm = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, NULL };

	ATF_REQUIRE_MSG(strptime(buf, fmt, &tm) == NULL, "strptime(\"%s\", "
	    "\"%s\", &tm) should fail, but it didn't", buf, fmt);
}

ATF_TC(common);

ATF_TC_HEAD(common, tc)
{

	atf_tc_set_md_var(tc, "descr", "Checks strptime(3): various checks");
}

ATF_TC_BODY(common, tc)
{

	h_pass("Tue Jan 20 23:27:46 1998", "%a %b %d %T %Y",
		24, 46, 27, 23, 20, 0, 98, 2, -1);
	h_pass("Tue Jan 20 23:27:46 1998", "%a %b %d %H:%M:%S %Y",
		24, 46, 27, 23, 20, 0, 98, 2, -1);
	h_pass("Tue Jan 20 23:27:46 1998", "%c",
		24, 46, 27, 23, 20, 0, 98, 2, -1);
	h_pass("Fri Mar  4 20:05:34 2005", "%a %b %e %H:%M:%S %Y",
		24, 34, 5, 20, 4, 2, 105, 5, -1);
	h_pass("5\t3  4 8pm:05:34 2005", "%w%n%m%t%d%n%k%p:%M:%S %Y",
		21, 34, 5, 20, 4, 2, 105, 5, -1);
	h_pass("Fri Mar  4 20:05:34 2005", "%c",
		24, 34, 5, 20, 4, 2, 105, 5, -1);

	h_pass("x20y", "x%Cy", 4, -1, -1, -1, -1, -1, 100, -1, -1);
	h_pass("x84y", "x%yy", 4, -1, -1, -1, -1, -1, 84, -1, -1);
	h_pass("x2084y", "x%C%yy", 6, -1, -1, -1, -1, -1, 184, -1, -1);
	h_pass("x8420y", "x%y%Cy", 6, -1, -1, -1, -1, -1, 184, -1, -1);
	h_pass("%20845", "%%%C%y5", 6, -1, -1, -1, -1, -1, 184, -1, -1);
	h_fail("%", "%E%");

	h_pass("1980", "%Y", 4, -1, -1, -1, -1, -1, 80, -1, -1);
	h_pass("1980", "%EY", 4, -1, -1, -1, -1, -1, 80, -1, -1);

	h_pass("0", "%S", 1, 0, -1, -1, -1, -1, -1, -1, -1);
	h_pass("59", "%S", 2, 59, -1, -1, -1, -1, -1, -1, -1);
	h_pass("60", "%S", 2, 60, -1, -1, -1, -1, -1, -1, -1);
	h_pass("61", "%S", 2, 61, -1, -1, -1, -1, -1, -1, -1);
	h_fail("62", "%S");
}

ATF_TC(day);

ATF_TC_HEAD(day, tc)
{

	atf_tc_set_md_var(tc, "descr", "Checks strptime(3): day names");
}

ATF_TC_BODY(day, tc)
{

	h_pass("Sun", "%a", 3, -1, -1, -1, -1, -1, -1, 0, -1);
	h_pass("Sunday", "%a", 6, -1, -1, -1, -1, -1, -1, 0, -1);
	h_pass("Mon", "%a", 3, -1, -1, -1, -1, -1, -1, 1, -1);
	h_pass("Monday", "%a", 6, -1, -1, -1, -1, -1, -1, 1, -1);
	h_pass("Tue", "%a", 3, -1, -1, -1, -1, -1, -1, 2, -1);
	h_pass("Tuesday", "%a", 7, -1, -1, -1, -1, -1, -1, 2, -1);
	h_pass("Wed", "%a", 3, -1, -1, -1, -1, -1, -1, 3, -1);
	h_pass("Wednesday", "%a", 9, -1, -1, -1, -1, -1, -1, 3, -1);
	h_pass("Thu", "%a", 3, -1, -1, -1, -1, -1, -1, 4, -1);
	h_pass("Thursday", "%a", 8, -1, -1, -1, -1, -1, -1, 4, -1);
	h_pass("Fri", "%a", 3, -1, -1, -1, -1, -1, -1, 5, -1);
	h_pass("Friday", "%a", 6, -1, -1, -1, -1, -1, -1, 5, -1);
	h_pass("Sat", "%a", 3, -1, -1, -1, -1, -1, -1, 6, -1);
	h_pass("Saturday", "%a", 8, -1, -1, -1, -1, -1, -1, 6, -1);
	h_pass("Saturn", "%a", 3, -1, -1, -1, -1, -1, -1, 6, -1);
	h_fail("Moon", "%a");
	h_pass("Sun", "%A", 3, -1, -1, -1, -1, -1, -1, 0, -1);
	h_pass("Sunday", "%A", 6, -1, -1, -1, -1, -1, -1, 0, -1);
	h_pass("Mon", "%A", 3, -1, -1, -1, -1, -1, -1, 1, -1);
	h_pass("Monday", "%A", 6, -1, -1, -1, -1, -1, -1, 1, -1);
	h_pass("Tue", "%A", 3, -1, -1, -1, -1, -1, -1, 2, -1);
	h_pass("Tuesday", "%A", 7, -1, -1, -1, -1, -1, -1, 2, -1);
	h_pass("Wed", "%A", 3, -1, -1, -1, -1, -1, -1, 3, -1);
	h_pass("Wednesday", "%A", 9, -1, -1, -1, -1, -1, -1, 3, -1);
	h_pass("Thu", "%A", 3, -1, -1, -1, -1, -1, -1, 4, -1);
	h_pass("Thursday", "%A", 8, -1, -1, -1, -1, -1, -1, 4, -1);
	h_pass("Fri", "%A", 3, -1, -1, -1, -1, -1, -1, 5, -1);
	h_pass("Friday", "%A", 6, -1, -1, -1, -1, -1, -1, 5, -1);
	h_pass("Sat", "%A", 3, -1, -1, -1, -1, -1, -1, 6, -1);
	h_pass("Saturday", "%A", 8, -1, -1, -1, -1, -1, -1, 6, -1);
	h_pass("Saturn", "%A", 3, -1, -1, -1, -1, -1, -1, 6, -1);
	h_fail("Moon", "%A");

	h_pass("mon", "%a", 3, -1, -1, -1, -1, -1, -1, 1, -1);
	h_pass("tueSDay", "%A", 7, -1, -1, -1, -1, -1, -1, 2, -1);
	h_pass("sunday", "%A", 6, -1, -1, -1, -1, -1, -1, 0, -1);
	h_fail("sunday", "%EA");
	h_pass("SaturDay", "%A", 8, -1, -1, -1, -1, -1, -1, 6, -1);
	h_fail("SaturDay", "%OA");
}

ATF_TC(month);

ATF_TC_HEAD(month, tc)
{

	atf_tc_set_md_var(tc, "descr", "Checks strptime(3): month names");
}

ATF_TC_BODY(month, tc)
{

	h_pass("Jan", "%b", 3, -1, -1, -1, -1, 0, -1, -1, -1);
	h_pass("January", "%b", 7, -1, -1, -1, -1, 0, -1, -1, -1);
	h_pass("Feb", "%b", 3, -1, -1, -1, -1, 1, -1, -1, -1);
	h_pass("February", "%b", 8, -1, -1, -1, -1, 1, -1, -1, -1);
	h_pass("Mar", "%b", 3, -1, -1, -1, -1, 2, -1, -1, -1);
	h_pass("March", "%b", 5, -1, -1, -1, -1, 2, -1, -1, -1);
	h_pass("Apr", "%b", 3, -1, -1, -1, -1, 3, -1, -1, -1);
	h_pass("April", "%b", 5, -1, -1, -1, -1, 3, -1, -1, -1);
	h_pass("May", "%b", 3, -1, -1, -1, -1, 4, -1, -1, -1);
	h_pass("Jun", "%b", 3, -1, -1, -1, -1, 5, -1, -1, -1);
	h_pass("June", "%b", 4, -1, -1, -1, -1, 5, -1, -1, -1);
	h_pass("Jul", "%b", 3, -1, -1, -1, -1, 6, -1, -1, -1);
	h_pass("July", "%b", 4, -1, -1, -1, -1, 6, -1, -1, -1);
	h_pass("Aug", "%b", 3, -1, -1, -1, -1, 7, -1, -1, -1);
	h_pass("August", "%b", 6, -1, -1, -1, -1, 7, -1, -1, -1);
	h_pass("Sep", "%b", 3, -1, -1, -1, -1, 8, -1, -1, -1);
	h_pass("September", "%b", 9, -1, -1, -1, -1, 8, -1, -1, -1);
	h_pass("Oct", "%b", 3, -1, -1, -1, -1, 9, -1, -1, -1);
	h_pass("October", "%b", 7, -1, -1, -1, -1, 9, -1, -1, -1);
	h_pass("Nov", "%b", 3, -1, -1, -1, -1, 10, -1, -1, -1);
	h_pass("November", "%b", 8, -1, -1, -1, -1, 10, -1, -1, -1);
	h_pass("Dec", "%b", 3, -1, -1, -1, -1, 11, -1, -1, -1);
	h_pass("December", "%b", 8, -1, -1, -1, -1, 11, -1, -1, -1);
	h_pass("Mayor", "%b", 3, -1, -1, -1, -1, 4, -1, -1, -1);
	h_pass("Mars", "%b", 3, -1, -1, -1, -1, 2, -1, -1, -1);
	h_fail("Rover", "%b");
	h_pass("Jan", "%B", 3, -1, -1, -1, -1, 0, -1, -1, -1);
	h_pass("January", "%B", 7, -1, -1, -1, -1, 0, -1, -1, -1);
	h_pass("Feb", "%B", 3, -1, -1, -1, -1, 1, -1, -1, -1);
	h_pass("February", "%B", 8, -1, -1, -1, -1, 1, -1, -1, -1);
	h_pass("Mar", "%B", 3, -1, -1, -1, -1, 2, -1, -1, -1);
	h_pass("March", "%B", 5, -1, -1, -1, -1, 2, -1, -1, -1);
	h_pass("Apr", "%B", 3, -1, -1, -1, -1, 3, -1, -1, -1);
	h_pass("April", "%B", 5, -1, -1, -1, -1, 3, -1, -1, -1);
	h_pass("May", "%B", 3, -1, -1, -1, -1, 4, -1, -1, -1);
	h_pass("Jun", "%B", 3, -1, -1, -1, -1, 5, -1, -1, -1);
	h_pass("June", "%B", 4, -1, -1, -1, -1, 5, -1, -1, -1);
	h_pass("Jul", "%B", 3, -1, -1, -1, -1, 6, -1, -1, -1);
	h_pass("July", "%B", 4, -1, -1, -1, -1, 6, -1, -1, -1);
	h_pass("Aug", "%B", 3, -1, -1, -1, -1, 7, -1, -1, -1);
	h_pass("August", "%B", 6, -1, -1, -1, -1, 7, -1, -1, -1);
	h_pass("Sep", "%B", 3, -1, -1, -1, -1, 8, -1, -1, -1);
	h_pass("September", "%B", 9, -1, -1, -1, -1, 8, -1, -1, -1);
	h_pass("Oct", "%B", 3, -1, -1, -1, -1, 9, -1, -1, -1);
	h_pass("October", "%B", 7, -1, -1, -1, -1, 9, -1, -1, -1);
	h_pass("Nov", "%B", 3, -1, -1, -1, -1, 10, -1, -1, -1);
	h_pass("November", "%B", 8, -1, -1, -1, -1, 10, -1, -1, -1);
	h_pass("Dec", "%B", 3, -1, -1, -1, -1, 11, -1, -1, -1);
	h_pass("December", "%B", 8, -1, -1, -1, -1, 11, -1, -1, -1);
	h_pass("Mayor", "%B", 3, -1, -1, -1, -1, 4, -1, -1, -1);
	h_pass("Mars", "%B", 3, -1, -1, -1, -1, 2, -1, -1, -1);
	h_fail("Rover", "%B");

	h_pass("september", "%b", 9, -1, -1, -1, -1, 8, -1, -1, -1);
	h_pass("septembe", "%B", 3, -1, -1, -1, -1, 8, -1, -1, -1);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, common);
	ATF_TP_ADD_TC(tp, day);
	ATF_TP_ADD_TC(tp, month);

	return atf_no_error();
}
