#include "config.h"

#include "lfptest.h"
#include "timevalops.h"

#include "unity.h"
#include <math.h>// Required on Solaris for ldexp.


void test_Seconds(void)
{
	struct timeval input = {500, 0}; // 500.0 s
	l_fp expected = {500, 0};
	l_fp actual;

	TVTOTS(&input, &actual);

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}

void test_MicrosecondsRounded(void)
{
	/* 0.0005 can not be represented exact in a l_fp structure.
	 * It would equal to 2147483,648. This means that
	 * HALF_PROMILLE_UP (which is 2147484) should be
	 * the correct rounding. */

	struct timeval input = {0, 500}; // 0.0005 exact
	l_fp expected = {0, HALF_PROMILLE_UP};
	l_fp actual;

	TVTOTS(&input, &actual);
	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}

void test_MicrosecondsExact(void)
{
	// 0.5 can be represented exact in both l_fp and timeval.
	const struct timeval input = {10, 500000}; // 0.5 exact
	const l_fp expected = {10, HALF}; // 0.5 exact
	l_fp actual;

	TVTOTS(&input, &actual);

	// Compare the fractional part with an absolute error given.
	TEST_ASSERT_EQUAL_UINT(expected.l_ui, actual.l_ui);

	double expectedDouble, actualDouble;
	M_LFPTOD(0, expected.l_uf, expectedDouble);
	M_LFPTOD(0, actual.l_uf, actualDouble);

	// The error should be less than 0.5 us
	TEST_ASSERT_DOUBLE_WITHIN(0000005, expectedDouble, actualDouble);
}
