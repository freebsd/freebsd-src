#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"

#include "unity.h"
#include "lfptest.h"

/* This file tests both atolfp and mstolfp */

void setUp(void);
void test_PositiveInteger(void);
void test_NegativeInteger(void);
void test_PositiveFraction(void);
void test_NegativeFraction(void);
void test_PositiveMsFraction(void);
void test_NegativeMsFraction(void);
void test_InvalidChars(void);


void
setUp(void)
{
	init_lib();

	return;
}


void test_PositiveInteger(void) {
	const char *str = "500";
	const char *str_ms = "500000";

	l_fp expected = {{500},0};
	l_fp actual, actual_ms;

	TEST_ASSERT_TRUE(atolfp(str, &actual));
	TEST_ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual_ms));
}

void test_NegativeInteger(void) {
	const char *str = "-300";
	const char *str_ms = "-300000";

	l_fp expected;
	expected.l_i = -300;
	expected.l_uf = 0;

	l_fp actual, actual_ms;

	TEST_ASSERT_TRUE(atolfp(str, &actual));
	TEST_ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual_ms));
}

void test_PositiveFraction(void) {
	const char *str = "+500.5";
	const char *str_ms = "500500.0";

	l_fp expected = {{500}, HALF};
	l_fp actual, actual_ms;

	TEST_ASSERT_TRUE(atolfp(str, &actual));
	TEST_ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual_ms));
}

void test_NegativeFraction(void) {
	const char *str = "-300.75";
	const char *str_ms = "-300750";

	l_fp expected;
	expected.l_i = -301;
	expected.l_uf = QUARTER;

	l_fp actual, actual_ms;

	TEST_ASSERT_TRUE(atolfp(str, &actual));
	TEST_ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual_ms));
}

void test_PositiveMsFraction(void) {
	const char *str = "300.00025";
	const char *str_ms = "300000.25";

	l_fp expected = {{300}, QUARTER_PROMILLE_APPRX};
	l_fp actual, actual_ms;


	TEST_ASSERT_TRUE(atolfp(str, &actual));
	TEST_ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual_ms));

}

void test_NegativeMsFraction(void) {
	const char *str = "-199.99975";
	const char *str_ms = "-199999.75";

	l_fp expected;
	expected.l_i = -200;
	expected.l_uf = QUARTER_PROMILLE_APPRX;

	l_fp actual, actual_ms;

	TEST_ASSERT_TRUE(atolfp(str, &actual));
	TEST_ASSERT_TRUE(mstolfp(str_ms, &actual_ms));

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
	TEST_ASSERT_TRUE(IsEqual(expected, actual_ms));

}

void test_InvalidChars(void) {
	const char *str = "500.4a2";
	l_fp actual, actual_ms;

	TEST_ASSERT_FALSE(atolfp(str, &actual));
	TEST_ASSERT_FALSE(mstolfp(str, &actual_ms));
}

