#include "config.h"
#include "ntp_calendar.h"
#include "unity.h"

void test_DateGivenMonthDay(void);
void test_DateGivenYearDay(void);
void test_DateLeapYear(void);
void test_WraparoundDateIn2036(void);

void
test_DateGivenMonthDay(void) {
	// 2010-06-24 12:50:00
	struct calendar input = {2010, 0, 6, 24, 12, 50, 0};

	u_long expected = 3486372600UL; // This is the timestamp above.

	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&input));
}

void
test_DateGivenYearDay(void) {
	// 2010-06-24 12:50:00
	// This is the 175th day of 2010.
	struct calendar input = {2010, 175, 0, 0, 12, 50, 0};

	u_long expected = 3486372600UL; // This is the timestamp above.

	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&input));
}

void
test_DateLeapYear(void) {
	// 2012-06-24 12:00:00
	// This is the 176th day of 2012 (since 2012 is a leap year).
	struct calendar inputYd = {2012, 176, 0, 0, 12, 00, 00};
	struct calendar inputMd = {2012, 0, 6, 24, 12, 00, 00};

	u_long expected = 3549528000UL;

	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&inputYd));
	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&inputMd));
}

void
test_WraparoundDateIn2036(void) {
	// 2036-02-07 06:28:16
	// This is (one) wrapping boundary where we go from ULONG_MAX to 0.
	struct calendar input = {2036, 0, 2, 7, 6, 28, 16};

	u_long expected = 0UL;

	TEST_ASSERT_EQUAL_UINT(expected, caltontp(&input));
}
