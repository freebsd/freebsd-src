#include "config.h"
#include "ntp_types.h"
#include "ntp_stdlib.h"

#include "lfptest.h"

#include "ntp_unixtime.h"

#include "unity.h"

// Required for Solaris.
#include <math.h>



void test_ZeroBuffer() {
#ifndef SYS_WINNT
	const struct timeval input = {0, 0};
	const l_fp expected = {0 + JAN_1970, 0};

	l_fp actual;

	TEST_ASSERT_TRUE(buftvtots((const char*)(&input), &actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual));
#else
	TEST_IGNORE_MESSAGE("Test only for Windows, skipping...");
#endif
}

void test_IntegerAndFractionalBuffer() {
#ifndef SYS_WINNT
	const struct timeval input = {5, 500000}; // 5.5
	const l_fp expected = {5 + JAN_1970, HALF};

	l_fp actual;

	TEST_ASSERT_TRUE(buftvtots((const char*)(&input), &actual));

	// Compare the fractional part with an absolute error given.
	TEST_ASSERT_EQUAL(expected.l_ui, actual.l_ui);

	double expectedDouble, actualDouble;
	M_LFPTOD(0, expected.l_uf, expectedDouble);
	M_LFPTOD(0, actual.l_uf, actualDouble);

	// The error should be less than 0.5 us
	TEST_ASSERT_DOUBLE_WITHIN(0.0000005,expectedDouble,actualDouble);  //delta,epected,actual //_EXPECT_NEAR(expectedDouble, actualDouble, 0.0000005);
#else
	TEST_IGNORE_MESSAGE("Test only for Windows, skipping...");
#endif
}

void test_IllegalMicroseconds() {
#ifndef SYS_WINNT
	const struct timeval input = {0, 1100000}; // > 999 999 microseconds.
	
	l_fp actual;

	TEST_ASSERT_FALSE(buftvtots((const char*)(&input), &actual));
#else
	TEST_IGNORE_MESSAGE("Test only for Windows, skipping...");
#endif
}


void test_AlwaysFalseOnWindows() {
#ifdef SYS_WINNT
	/*
	 * Under Windows, buftvtots will just return
	 * 0 (false).
	 */
	l_fp actual;
	TEST_ASSERT_FALSE(buftvtots("", &actual));
#else
	TEST_IGNORE_MESSAGE("Non-Windows test, skipping...");
#endif
}

