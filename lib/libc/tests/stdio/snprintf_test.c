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

#include <atf-c.h>

#define SNPRINTF_TEST(output, format, ...) \
	do {								\
		char buf[256];						\
		assert(output == NULL || strlen(output) < sizeof(buf));	\
		int ret = snprintf(buf, sizeof(buf), format, __VA_ARGS__); \
		if (output == NULL) {					\
			ATF_CHECK_EQ(-1, ret);				\
		} else {						\
			ATF_CHECK_EQ(strlen(output), ret);		\
			if (ret > 0) {					\
				ATF_CHECK_STREQ(output, buf);		\
			}						\
		}							\
	} while (0)

ATF_TC_WITHOUT_HEAD(snprintf_b);
ATF_TC_BODY(snprintf_b, tc)
{
	SNPRINTF_TEST("0", "%b", 0);
	SNPRINTF_TEST("           0", "%12b", 0);
	SNPRINTF_TEST("000000000000", "%012b", 0);
	SNPRINTF_TEST("1", "%b", 1);
	SNPRINTF_TEST("           1", "%12b", 1);
	SNPRINTF_TEST("000000000001", "%012b", 1);
	SNPRINTF_TEST("1111111111111111111111111111111", "%b", INT_MAX);
	SNPRINTF_TEST("0", "%#b", 0);
	SNPRINTF_TEST("           0", "%#12b", 0);
	SNPRINTF_TEST("000000000000", "%#012b", 0);
	SNPRINTF_TEST("0b1", "%#b", 1);
	SNPRINTF_TEST("         0b1", "%#12b", 1);
	SNPRINTF_TEST("0b0000000001", "%#012b", 1);
	SNPRINTF_TEST("0b1111111111111111111111111111111", "%#b", INT_MAX);
}

ATF_TC_WITHOUT_HEAD(snprintf_B);
ATF_TC_BODY(snprintf_B, tc)
{
	SNPRINTF_TEST("0", "%B", 0);
	SNPRINTF_TEST("           0", "%12B", 0);
	SNPRINTF_TEST("000000000000", "%012B", 0);
	SNPRINTF_TEST("1", "%B", 1);
	SNPRINTF_TEST("           1", "%12B", 1);
	SNPRINTF_TEST("000000000001", "%012B", 1);
	SNPRINTF_TEST("1111111111111111111111111111111", "%B", INT_MAX);
	SNPRINTF_TEST("0", "%#B", 0);
	SNPRINTF_TEST("           0", "%#12B", 0);
	SNPRINTF_TEST("000000000000", "%#012B", 0);
	SNPRINTF_TEST("0B1", "%#B", 1);
	SNPRINTF_TEST("         0B1", "%#12B", 1);
	SNPRINTF_TEST("0B0000000001", "%#012B", 1);
	SNPRINTF_TEST("0B1111111111111111111111111111111", "%#B", INT_MAX);
}

ATF_TC_WITHOUT_HEAD(snprintf_d);
ATF_TC_BODY(snprintf_d, tc)
{
	SNPRINTF_TEST("0", "%d", 0);
	SNPRINTF_TEST("           0", "%12d", 0);
	SNPRINTF_TEST("000000000000", "%012d", 0);
	SNPRINTF_TEST("1", "%d", 1);
	SNPRINTF_TEST("           1", "%12d", 1);
	SNPRINTF_TEST("000000000001", "%012d", 1);
	SNPRINTF_TEST("2147483647", "%d", INT_MAX);
	SNPRINTF_TEST("  2147483647", "%12d", INT_MAX);
	SNPRINTF_TEST("002147483647", "%012d", INT_MAX);
	SNPRINTF_TEST("2,147,483,647", "%'d", INT_MAX);
}

ATF_TC_WITHOUT_HEAD(snprintf_x);
ATF_TC_BODY(snprintf_x, tc)
{
	SNPRINTF_TEST("0", "%x", 0);
	SNPRINTF_TEST("           0", "%12x", 0);
	SNPRINTF_TEST("000000000000", "%012x", 0);
	SNPRINTF_TEST("1", "%x", 1);
	SNPRINTF_TEST("           1", "%12x", 1);
	SNPRINTF_TEST("000000000001", "%012x", 1);
	SNPRINTF_TEST("7fffffff", "%x", INT_MAX);
	SNPRINTF_TEST("    7fffffff", "%12x", INT_MAX);
	SNPRINTF_TEST("00007fffffff", "%012x", INT_MAX);
	SNPRINTF_TEST("0", "%#x", 0);
	SNPRINTF_TEST("           0", "%#12x", 0);
	SNPRINTF_TEST("000000000000", "%#012x", 0);
	SNPRINTF_TEST("0x1", "%#x", 1);
	SNPRINTF_TEST("         0x1", "%#12x", 1);
	SNPRINTF_TEST("0x0000000001", "%#012x", 1);
	SNPRINTF_TEST("0x7fffffff", "%#x", INT_MAX);
	SNPRINTF_TEST("  0x7fffffff", "%#12x", INT_MAX);
	SNPRINTF_TEST("0x007fffffff", "%#012x", INT_MAX);
}

ATF_TC_WITHOUT_HEAD(snprintf_X);
ATF_TC_BODY(snprintf_X, tc)
{
	SNPRINTF_TEST("0", "%X", 0);
	SNPRINTF_TEST("           0", "%12X", 0);
	SNPRINTF_TEST("000000000000", "%012X", 0);
	SNPRINTF_TEST("1", "%X", 1);
	SNPRINTF_TEST("           1", "%12X", 1);
	SNPRINTF_TEST("000000000001", "%012X", 1);
	SNPRINTF_TEST("7FFFFFFF", "%X", INT_MAX);
	SNPRINTF_TEST("    7FFFFFFF", "%12X", INT_MAX);
	SNPRINTF_TEST("00007FFFFFFF", "%012X", INT_MAX);
	SNPRINTF_TEST("0", "%#X", 0);
	SNPRINTF_TEST("           0", "%#12X", 0);
	SNPRINTF_TEST("000000000000", "%#012X", 0);
	SNPRINTF_TEST("0X1", "%#X", 1);
	SNPRINTF_TEST("         0X1", "%#12X", 1);
	SNPRINTF_TEST("0X0000000001", "%#012X", 1);
	SNPRINTF_TEST("0X7FFFFFFF", "%#X", INT_MAX);
	SNPRINTF_TEST("  0X7FFFFFFF", "%#12X", INT_MAX);
	SNPRINTF_TEST("0X007FFFFFFF", "%#012X", INT_MAX);
}

ATF_TP_ADD_TCS(tp)
{
	setlocale(LC_NUMERIC, "en_US.UTF-8");
	ATF_TP_ADD_TC(tp, snprintf_b);
	ATF_TP_ADD_TC(tp, snprintf_B);
	ATF_TP_ADD_TC(tp, snprintf_d);
	ATF_TP_ADD_TC(tp, snprintf_x);
	ATF_TP_ADD_TC(tp, snprintf_X);
	return (atf_no_error());
}
