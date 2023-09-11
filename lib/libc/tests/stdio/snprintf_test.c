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

#ifndef nitems
#define nitems(a) (sizeof(a) / sizeof(a[0]))
#endif

#define SNPRINTF_TEST(output, format, ...)				\
	do {								\
		char buf[256];						\
		assert(strlen(output) < nitems(buf));			\
		int ret = snprintf(buf, nitems(buf), format,		\
		    __VA_ARGS__);					\
		ATF_CHECK_EQ(strlen(output), ret);			\
		if (ret > 0) {						\
			ATF_CHECK_EQ(0, strcmp(output, buf));		\
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

ATF_TC_WITHOUT_HEAD(snprintf_wN);
ATF_TC_BODY(snprintf_wN, tc)
{
	SNPRINTF_TEST("0", "%w8d", (int8_t)0);
	SNPRINTF_TEST("-128", "%w8d", (int8_t)SCHAR_MIN);
	SNPRINTF_TEST("127", "%w8d", (int8_t)SCHAR_MAX);
	SNPRINTF_TEST("0", "%w8u", (uint8_t)0);
	SNPRINTF_TEST("255", "%w8u", (uint8_t)UCHAR_MAX);

	SNPRINTF_TEST("0", "%w16d", (int16_t)0);
	SNPRINTF_TEST("-32768", "%w16d", (int16_t)SHRT_MIN);
	SNPRINTF_TEST("32767", "%w16d", (int16_t)SHRT_MAX);
	SNPRINTF_TEST("0", "%w16u", (uint16_t)0);
	SNPRINTF_TEST("65535", "%w16u", (uint16_t)USHRT_MAX);

	SNPRINTF_TEST("0", "%w32d", (int32_t)0);
	SNPRINTF_TEST("-2147483648", "%w32d", (int32_t)INT_MIN);
	SNPRINTF_TEST("2147483647", "%w32d", (int32_t)INT_MAX);
	SNPRINTF_TEST("0", "%w32u", (uint32_t)0);
	SNPRINTF_TEST("4294967295", "%w32u", (uint32_t)UINT_MAX);

	SNPRINTF_TEST("0", "%w64d", (int64_t)0);
	SNPRINTF_TEST("-9223372036854775808", "%w64d", (int64_t)LLONG_MIN);
	SNPRINTF_TEST("9223372036854775807", "%w64d", (int64_t)LLONG_MAX);
	SNPRINTF_TEST("0", "%w64u", (uint64_t)0);
	SNPRINTF_TEST("18446744073709551615", "%w64u", (uint64_t)ULLONG_MAX);

	SNPRINTF_TEST("wd", "%wd", 0);
	SNPRINTF_TEST("w1d", "%w1d", 0);
	SNPRINTF_TEST("w128d", "%w128d", 0);
}

ATF_TC_WITHOUT_HEAD(snprintf_wfN);
ATF_TC_BODY(snprintf_wfN, tc)
{
	SNPRINTF_TEST("0", "%wf8d", (int_fast8_t)0);
	SNPRINTF_TEST("-2147483648", "%wf8d", (int_fast8_t)INT_MIN);
	SNPRINTF_TEST("2147483647", "%wf8d", (int_fast8_t)INT_MAX);
	SNPRINTF_TEST("0", "%wf8u", (uint8_t)0);
	SNPRINTF_TEST("4294967295", "%wf8u", (uint_fast8_t)UINT_MAX);

	SNPRINTF_TEST("0", "%wf16d", (int_fast16_t)0);
	SNPRINTF_TEST("-2147483648", "%wf16d", (int_fast16_t)INT_MIN);
	SNPRINTF_TEST("2147483647", "%wf16d", (int_fast16_t)INT_MAX);
	SNPRINTF_TEST("0", "%wf16u", (uint16_t)0);
	SNPRINTF_TEST("4294967295", "%wf16u", (uint_fast16_t)UINT_MAX);

	SNPRINTF_TEST("0", "%wf32d", (int_fast32_t)0);
	SNPRINTF_TEST("-2147483648", "%wf32d", (int_fast32_t)INT_MIN);
	SNPRINTF_TEST("2147483647", "%wf32d", (int_fast32_t)INT_MAX);
	SNPRINTF_TEST("0", "%wf32u", (uint32_t)0);
	SNPRINTF_TEST("4294967295", "%wf32u", (uint_fast32_t)UINT_MAX);

	SNPRINTF_TEST("0", "%wf64d", (int_fast64_t)0);
	SNPRINTF_TEST("-9223372036854775808", "%wf64d", (int_fast64_t)LLONG_MIN);
	SNPRINTF_TEST("9223372036854775807", "%wf64d", (int_fast64_t)LLONG_MAX);
	SNPRINTF_TEST("0", "%wf64u", (uint64_t)0);
	SNPRINTF_TEST("18446744073709551615", "%wf64u", (uint_fast64_t)ULLONG_MAX);

	SNPRINTF_TEST("wfd", "%wfd", 0);
	SNPRINTF_TEST("wf1d", "%wf1d", 0);
	SNPRINTF_TEST("wf128d", "%wf128d", 0);
}

ATF_TP_ADD_TCS(tp)
{
	setlocale(LC_NUMERIC, "en_US.UTF-8");
	ATF_TP_ADD_TC(tp, snprintf_b);
	ATF_TP_ADD_TC(tp, snprintf_B);
	ATF_TP_ADD_TC(tp, snprintf_d);
	ATF_TP_ADD_TC(tp, snprintf_x);
	ATF_TP_ADD_TC(tp, snprintf_X);
	ATF_TP_ADD_TC(tp, snprintf_wN);
	ATF_TP_ADD_TC(tp, snprintf_wfN);
	return (atf_no_error());
}
