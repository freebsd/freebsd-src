#include "lfptest.h"

/* This class tests both atolfp and mstolfp */

class strtolfpTest : public lfptest {
};

TEST_F(strtolfpTest, PositiveInteger) {
	const char *str = "500";
	const char *str_ms = "500000";

	l_fp expected = {500,0};
	l_fp actual, actual_ms;

	ASSERT_TRUE(atolfp(str, &actual));
	ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	EXPECT_TRUE(IsEqual(expected, actual));
	EXPECT_TRUE(IsEqual(expected, actual_ms));
}

TEST_F(strtolfpTest, NegativeInteger) {
	const char *str = "-300";
	const char *str_ms = "-300000";

	l_fp expected;
	expected.l_i = -300;
	expected.l_uf = 0;

	l_fp actual, actual_ms;

	ASSERT_TRUE(atolfp(str, &actual));
	ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	EXPECT_TRUE(IsEqual(expected, actual));
	EXPECT_TRUE(IsEqual(expected, actual_ms));
}

TEST_F(strtolfpTest, PositiveFraction) {
	const char *str = "+500.5";
	const char *str_ms = "500500.0";

	l_fp expected = {500, HALF};
	l_fp actual, actual_ms;

	ASSERT_TRUE(atolfp(str, &actual));
	ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	EXPECT_TRUE(IsEqual(expected, actual));
	EXPECT_TRUE(IsEqual(expected, actual_ms));
}

TEST_F(strtolfpTest, NegativeFraction) {
	const char *str = "-300.75";
	const char *str_ms = "-300750";

	l_fp expected;
	expected.l_i = -301;
	expected.l_uf = QUARTER;

	l_fp actual, actual_ms;

	ASSERT_TRUE(atolfp(str, &actual));
	ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	EXPECT_TRUE(IsEqual(expected, actual));
	EXPECT_TRUE(IsEqual(expected, actual_ms));
}

TEST_F(strtolfpTest, PositiveMsFraction) {
	const char *str = "300.00025";
	const char *str_ms = "300000.25";

	l_fp expected = {300, QUARTER_PROMILLE_APPRX};
	l_fp actual, actual_ms;

	ASSERT_TRUE(atolfp(str, &actual));
	ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	EXPECT_TRUE(IsEqual(expected, actual));
	EXPECT_TRUE(IsEqual(expected, actual_ms));
}

TEST_F(strtolfpTest, NegativeMsFraction) {
	const char *str = "-199.99975";
	const char *str_ms = "-199999.75";

	l_fp expected;
	expected.l_i = -200;
	expected.l_uf = QUARTER_PROMILLE_APPRX;

	l_fp actual, actual_ms;

	ASSERT_TRUE(atolfp(str, &actual));
	ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	EXPECT_TRUE(IsEqual(expected, actual));
	EXPECT_TRUE(IsEqual(expected, actual_ms));
}

TEST_F(strtolfpTest, InvalidChars) {
	const char *str = "500.4a2";
	l_fp actual, actual_ms;

	ASSERT_FALSE(atolfp(str, &actual));
	ASSERT_FALSE(mstolfp(str, &actual_ms));
}
