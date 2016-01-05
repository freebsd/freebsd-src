#include "config.h"

#include "ntp_stdlib.h" //test fail without this include, for some reason
#include "ntp_calendar.h"
#include "unity.h"

#include "test-libntp.h"


void setUp()
{
    ntpcal_set_timefunc(timefunc);
    settime(1970, 1, 1, 0, 0, 0);
}

void tearDown()
{
    ntpcal_set_timefunc(NULL);
}


void test_NoWrapInDateRange() {
	const u_int32 input = 3486372600UL; // 2010-06-24 12:50:00.
	const u_int32 expected = 3471292800UL; // 2010-01-01 00:00:00

	TEST_ASSERT_EQUAL(expected, calyearstart(input, &nowtime));
	TEST_ASSERT_EQUAL(expected, calyearstart(input, NULL));
}

void test_NoWrapInDateRangeLeapYear() {
	const u_int32 input = 3549528000UL; // 2012-06-24 12:00:00
	const u_int32 expected = 3534364800UL; // 2012-01-01 00:00:00

	TEST_ASSERT_EQUAL(expected, calyearstart(input, &nowtime));
	TEST_ASSERT_EQUAL(expected, calyearstart(input, NULL));
}

void test_WrapInDateRange() {
	const u_int32 input = 19904UL; // 2036-02-07 12:00:00
	const u_int32 expected = 4291747200UL; // 2036-01-01 00:00:00

	TEST_ASSERT_EQUAL(expected, calyearstart(input, &nowtime));
	TEST_ASSERT_EQUAL(expected, calyearstart(input, NULL));
}
