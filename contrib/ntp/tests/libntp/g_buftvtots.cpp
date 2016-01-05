#include "g_lfptest.h"

extern "C" {
#include "ntp_unixtime.h"
};

// Required for Solaris.
#include <math.h>

class buftvtotsTest : public lfptest {
};

#ifndef SYS_WINNT
TEST_F(buftvtotsTest, ZeroBuffer) {
	const timeval input = {0, 0};
	const l_fp expected = {0 + JAN_1970, 0};

	l_fp actual;

	ASSERT_TRUE(buftvtots((const char*)(&input), &actual));
	EXPECT_TRUE(IsEqual(expected, actual));
}

TEST_F(buftvtotsTest, IntegerAndFractionalBuffer) {
	const timeval input = {5, 500000}; // 5.5
	const l_fp expected = {5 + JAN_1970, HALF};

	l_fp actual;

	ASSERT_TRUE(buftvtots((const char*)(&input), &actual));

	// Compare the fractional part with an absolute error given.
	EXPECT_EQ(expected.l_ui, actual.l_ui);

	double expectedDouble, actualDouble;
	M_LFPTOD(0, expected.l_uf, expectedDouble);
	M_LFPTOD(0, actual.l_uf, actualDouble);

	// The error should be less than 0.5 us
	EXPECT_NEAR(expectedDouble, actualDouble, 0.0000005);
}

TEST_F(buftvtotsTest, IllegalMicroseconds) {
	const timeval input = {0, 1100000}; // > 999 999 microseconds.
	
	l_fp actual;

	ASSERT_FALSE(buftvtots((const char*)(&input), &actual));
}

#else
TEST_F(buftvtotsTest, AlwaysFalseOnWindows) {
	/*
	 * Under Windows, buftvtots will just return
	 * 0 (false).
	 */
	l_fp actual;
	ASSERT_FALSE(buftvtots("", &actual));
}

#endif
