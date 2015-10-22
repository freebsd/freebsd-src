#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"
#include "ntp_fp.h"

#include "unity.h"

void test_RegularPositive(void);
void test_PositiveOverflowBoundary(void);
void test_PositiveOverflowBig(void);
void test_Negative(void);
void test_IllegalChar(void);



void test_RegularPositive(void) {
	const char *str = "305";
	u_long actual;

	TEST_ASSERT_TRUE(atouint(str, &actual));
	TEST_ASSERT_EQUAL(305, actual);
}

void test_PositiveOverflowBoundary(void) {
	const char *str = "4294967296";
	u_long actual;

	TEST_ASSERT_FALSE(atouint(str, &actual));
}

void test_PositiveOverflowBig(void) {
	const char *str = "8000000000";
	u_long actual;

	TEST_ASSERT_FALSE(atouint(str, &actual));
}

void test_Negative(void) {
	const char *str = "-1";
	u_long actual;

	TEST_ASSERT_FALSE(atouint(str, &actual));
}

void test_IllegalChar(void) {
	const char *str = "50c3";
	u_long actual;

	TEST_ASSERT_FALSE(atouint(str, &actual));
}
