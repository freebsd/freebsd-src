#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"

#include "unity.h"

void setUp(void)
{ 
}

void tearDown(void)
{
}


void test_NonLeapYearFebruary (void) {
	TEST_ASSERT_EQUAL(31+20, ymd2yd(2010,2,20)); //2010-02-20
}

void test_NonLeapYearJune (void) {
	int expected = 31+28+31+30+31+18; // 18 June non-leap year
	TEST_ASSERT_EQUAL(expected, ymd2yd(2011,6,18));
}

void test_LeapYearFebruary (void) {
	TEST_ASSERT_EQUAL(31+20, ymd2yd(2012,2,20)); //2012-02-20 (leap year)
}

void test_LeapYearDecember (void) {
	// 2012-12-31
	int expected = 31+29+31+30+31+30+31+31+30+31+30+31;
	TEST_ASSERT_EQUAL(expected, ymd2yd(2012,12,31));
}

