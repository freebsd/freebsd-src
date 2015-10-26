#include "config.h"

#include "ntp_calendar.h"
#include "ntp_stdlib.h"

#include "unity.h"
#include "test-libntp.h"

#include <string.h>


char * CalendarToString(const struct calendar cal);
int IsEqual(const struct calendar expected, const struct calendar actual);
void setUp(void);
void tearDown(void);
void test_RegularTime(void);
void test_LeapYear(void);
void test_uLongBoundary(void);
void test_uLongWrapped(void);


char *
CalendarToString(const struct calendar cal) {
	char * str = emalloc (sizeof (char) * 100);
	
	char buffer[100] ="";
	snprintf(buffer, 100, "%u", cal.year);
	strcat(str, buffer);
	strcat(str, "-");
	snprintf(buffer, 100, "%u", (u_int)cal.month);
	strcat(str, buffer);
	strcat(str, "-");
	snprintf(buffer, 100, "%u", (u_int)cal.monthday);
	strcat(str, buffer);
	strcat(str, " (");
	snprintf(buffer, 100, "%u", (u_int) cal.yearday);
	strcat(str, buffer);
	strcat(str, ") ");
	snprintf(buffer, 100, "%u", (u_int)cal.hour);
	strcat(str, buffer);
	strcat(str, ":");
	snprintf(buffer, 100, "%u", (u_int)cal.minute);
	strcat(str, buffer);
	strcat(str, ":");
	snprintf(buffer, 100, "%u", (u_int)cal.second);
	strcat(str, buffer);
	return str;
}

int // technically boolean
IsEqual(const struct calendar expected, const struct calendar actual) {
	if (expected.year == actual.year &&
		(expected.yearday == actual.yearday ||
		 (expected.month == actual.month &&
		  expected.monthday == actual.monthday)) &&
		expected.hour == actual.hour &&
		expected.minute == actual.minute &&
		expected.second == actual.second) {
		return TRUE;
	} else {
		printf("expected: %s but was %s", CalendarToString(expected) ,CalendarToString(actual));
		return FALSE;
			
	}
}


void
setUp()
{
    ntpcal_set_timefunc(timefunc);
    settime(1970, 1, 1, 0, 0, 0);
}

void
tearDown()
{
    ntpcal_set_timefunc(NULL);
}


void
test_RegularTime(void) {
	u_long testDate = 3485080800UL; // 2010-06-09 14:00:00
	struct calendar expected = {2010,160,6,9,14,0,0};

	struct calendar actual;

	caljulian(testDate, &actual);

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}

void
test_LeapYear(void) {
	u_long input = 3549902400UL; // 2012-06-28 20:00:00Z
	struct calendar expected = {2012, 179, 6, 28, 20, 0, 0};

	struct calendar actual;

	caljulian(input, &actual);

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}

void
test_uLongBoundary(void) {
	u_long time = 4294967295UL; // 2036-02-07 6:28:15
	struct calendar expected = {2036,0,2,7,6,28,15};

	struct calendar actual;

	caljulian(time, &actual);

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}

void
test_uLongWrapped(void) {
	u_long time = 0;
	struct calendar expected = {2036,0,2,7,6,28,16};

	struct calendar actual;

	caljulian(time, &actual);

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}
