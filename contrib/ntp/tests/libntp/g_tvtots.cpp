#include "g_lfptest.h"

extern "C" {
#include "timevalops.h"
};

// Required on Solaris for ldexp.
#include <math.h>

class tvtotsTest : public lfptest {
};

TEST_F(tvtotsTest, Seconds) {
	timeval input = {500, 0}; // 500.0 s
	l_fp expected = {500, 0};
	l_fp actual;

	TVTOTS(&input, &actual);

	EXPECT_TRUE(IsEqual(expected, actual));
}

TEST_F(tvtotsTest, MicrosecondsRounded) {
	/* 0.0005 can not be represented exact in a l_fp structure.
	 * It would equal to 2147483,648. This means that
	 * HALF_PROMILLE_UP (which is 2147484) should be
	 * the correct rounding. */

	timeval input = {0, 500}; // 0.0005 exact
	l_fp expected = {0, HALF_PROMILLE_UP};
	l_fp actual;

	TVTOTS(&input, &actual);
	EXPECT_TRUE(IsEqual(expected, actual));
}

TEST_F(tvtotsTest, MicrosecondsExact) {
	// 0.5 can be represented exact in both l_fp and timeval.
	const timeval input = {10, 500000}; // 0.5 exact
	const l_fp expected = {10, HALF}; // 0.5 exact
	l_fp actual;

	TVTOTS(&input, &actual);

	// Compare the fractional part with an absolute error given.
	EXPECT_EQ(expected.l_ui, actual.l_ui);

	double expectedDouble, actualDouble;
	M_LFPTOD(0, expected.l_uf, expectedDouble);
	M_LFPTOD(0, actual.l_uf, actualDouble);

	// The error should be less than 0.5 us
	EXPECT_NEAR(expectedDouble, actualDouble, 0.0000005);
}
