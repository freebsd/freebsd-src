/*-
 * Copyright (c) 2023 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <limits.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

#include <atf-c.h>

#ifndef nitems
#define nitems(a) (sizeof(a) / sizeof(a[0]))
#endif

#define SWPRINTF_TEST(output, format, ...)				\
	do {								\
		wchar_t buf[256];					\
		assert(wcslen(L##output) < nitems(buf));		\
		int ret = swprintf(buf, nitems(buf), L##format,		\
		    __VA_ARGS__);					\
		ATF_CHECK_EQ(wcslen(L##output), ret);			\
		if (ret > 0) {						\
			ATF_CHECK_EQ(0, wcscmp(L##output, buf));	\
		}							\
	} while (0)

ATF_TC_WITHOUT_HEAD(swprintf_b);
ATF_TC_BODY(swprintf_b, tc)
{
	SWPRINTF_TEST("0", "%b", 0);
	SWPRINTF_TEST("           0", "%12b", 0);
	SWPRINTF_TEST("000000000000", "%012b", 0);
	SWPRINTF_TEST("1", "%b", 1);
	SWPRINTF_TEST("           1", "%12b", 1);
	SWPRINTF_TEST("000000000001", "%012b", 1);
	SWPRINTF_TEST("1111111111111111111111111111111", "%b", INT_MAX);
	SWPRINTF_TEST("0", "%#b", 0);
	SWPRINTF_TEST("           0", "%#12b", 0);
	SWPRINTF_TEST("000000000000", "%#012b", 0);
	SWPRINTF_TEST("0b1", "%#b", 1);
	SWPRINTF_TEST("         0b1", "%#12b", 1);
	SWPRINTF_TEST("0b0000000001", "%#012b", 1);
	SWPRINTF_TEST("0b1111111111111111111111111111111", "%#b", INT_MAX);
}

ATF_TC_WITHOUT_HEAD(swprintf_B);
ATF_TC_BODY(swprintf_B, tc)
{
	SWPRINTF_TEST("0", "%B", 0);
	SWPRINTF_TEST("           0", "%12B", 0);
	SWPRINTF_TEST("000000000000", "%012B", 0);
	SWPRINTF_TEST("1", "%B", 1);
	SWPRINTF_TEST("           1", "%12B", 1);
	SWPRINTF_TEST("000000000001", "%012B", 1);
	SWPRINTF_TEST("1111111111111111111111111111111", "%B", INT_MAX);
	SWPRINTF_TEST("0", "%#B", 0);
	SWPRINTF_TEST("           0", "%#12B", 0);
	SWPRINTF_TEST("000000000000", "%#012B", 0);
	SWPRINTF_TEST("0B1", "%#B", 1);
	SWPRINTF_TEST("         0B1", "%#12B", 1);
	SWPRINTF_TEST("0B0000000001", "%#012B", 1);
	SWPRINTF_TEST("0B1111111111111111111111111111111", "%#B", INT_MAX);
}

ATF_TC_WITHOUT_HEAD(swprintf_d);
ATF_TC_BODY(swprintf_d, tc)
{
	SWPRINTF_TEST("0", "%d", 0);
	SWPRINTF_TEST("           0", "%12d", 0);
	SWPRINTF_TEST("000000000000", "%012d", 0);
	SWPRINTF_TEST("1", "%d", 1);
	SWPRINTF_TEST("           1", "%12d", 1);
	SWPRINTF_TEST("000000000001", "%012d", 1);
	SWPRINTF_TEST("2147483647", "%d", INT_MAX);
	SWPRINTF_TEST("  2147483647", "%12d", INT_MAX);
	SWPRINTF_TEST("002147483647", "%012d", INT_MAX);
	SWPRINTF_TEST("2,147,483,647", "%'d", INT_MAX);
}

ATF_TC_WITHOUT_HEAD(swprintf_x);
ATF_TC_BODY(swprintf_x, tc)
{
	SWPRINTF_TEST("0", "%x", 0);
	SWPRINTF_TEST("           0", "%12x", 0);
	SWPRINTF_TEST("000000000000", "%012x", 0);
	SWPRINTF_TEST("1", "%x", 1);
	SWPRINTF_TEST("           1", "%12x", 1);
	SWPRINTF_TEST("000000000001", "%012x", 1);
	SWPRINTF_TEST("7fffffff", "%x", INT_MAX);
	SWPRINTF_TEST("    7fffffff", "%12x", INT_MAX);
	SWPRINTF_TEST("00007fffffff", "%012x", INT_MAX);
	SWPRINTF_TEST("0", "%#x", 0);
	SWPRINTF_TEST("           0", "%#12x", 0);
	SWPRINTF_TEST("000000000000", "%#012x", 0);
	SWPRINTF_TEST("0x1", "%#x", 1);
	SWPRINTF_TEST("         0x1", "%#12x", 1);
	SWPRINTF_TEST("0x0000000001", "%#012x", 1);
	SWPRINTF_TEST("0x7fffffff", "%#x", INT_MAX);
	SWPRINTF_TEST("  0x7fffffff", "%#12x", INT_MAX);
	SWPRINTF_TEST("0x007fffffff", "%#012x", INT_MAX);
}

ATF_TC_WITHOUT_HEAD(swprintf_X);
ATF_TC_BODY(swprintf_X, tc)
{
	SWPRINTF_TEST("0", "%X", 0);
	SWPRINTF_TEST("           0", "%12X", 0);
	SWPRINTF_TEST("000000000000", "%012X", 0);
	SWPRINTF_TEST("1", "%X", 1);
	SWPRINTF_TEST("           1", "%12X", 1);
	SWPRINTF_TEST("000000000001", "%012X", 1);
	SWPRINTF_TEST("7FFFFFFF", "%X", INT_MAX);
	SWPRINTF_TEST("    7FFFFFFF", "%12X", INT_MAX);
	SWPRINTF_TEST("00007FFFFFFF", "%012X", INT_MAX);
	SWPRINTF_TEST("0", "%#X", 0);
	SWPRINTF_TEST("           0", "%#12X", 0);
	SWPRINTF_TEST("000000000000", "%#012X", 0);
	SWPRINTF_TEST("0X1", "%#X", 1);
	SWPRINTF_TEST("         0X1", "%#12X", 1);
	SWPRINTF_TEST("0X0000000001", "%#012X", 1);
	SWPRINTF_TEST("0X7FFFFFFF", "%#X", INT_MAX);
	SWPRINTF_TEST("  0X7FFFFFFF", "%#12X", INT_MAX);
	SWPRINTF_TEST("0X007FFFFFFF", "%#012X", INT_MAX);
}

ATF_TP_ADD_TCS(tp)
{
	setlocale(LC_NUMERIC, "en_US.UTF-8");
	ATF_TP_ADD_TC(tp, swprintf_b);
	ATF_TP_ADD_TC(tp, swprintf_B);
	ATF_TP_ADD_TC(tp, swprintf_d);
	ATF_TP_ADD_TC(tp, swprintf_x);
	ATF_TP_ADD_TC(tp, swprintf_X);
	return (atf_no_error());
}
