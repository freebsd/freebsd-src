#include "config.h"

#include "ntp_calendar.h"
#include "ntp_stdlib.h"
#include "unity.h"

#include "test-libntp.h"


#include <string.h>
//#include <stdlib.h>

//added struct to calendar!

char * CalendarToString(const struct calendar cal) {
	char * ss = malloc (sizeof (char) * 100);
	
	char buffer[100] ="";
	sprintf(buffer, "%u", cal.year);
	strcat(ss,buffer);
	strcat(ss,"-");
	sprintf(buffer, "%u", (u_int)cal.month);
	strcat(ss,buffer);
	strcat(ss,"-");
	sprintf(buffer, "%u", (u_int)cal.monthday);
	strcat(ss,buffer);
	strcat(ss," (");
	sprintf(buffer, "%u", (u_int) cal.yearday);
	strcat(ss,buffer);
	strcat(ss,") ");
	sprintf(buffer, "%u", (u_int)cal.hour);
	strcat(ss,buffer);
	strcat(ss,":");
	sprintf(buffer, "%u", (u_int)cal.minute);
	strcat(ss,buffer);
	strcat(ss,":");
	sprintf(buffer, "%u", (u_int)cal.second);
	strcat(ss,buffer);
	//ss << cal.year << "-" << (u_int)cal.month << "-" << (u_int)cal.monthday << " (" << cal.yearday << ") " << (u_int)cal.hour << ":" << (u_int)cal.minute << ":" << (u_int)cal.second;
	return ss;
}

//tehnically boolean
int IsEqual(const struct calendar expected, const struct calendar actual) {
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


void setUp()
{

    ntpcal_set_timefunc(timefunc);
    settime(1970, 1, 1, 0, 0, 0);
}

void tearDown()
{
    ntpcal_set_timefunc(NULL);
}


void test_RegularTime() {
	u_long testDate = 3485080800UL; // 2010-06-09 14:00:00
	struct calendar expected = {2010,160,6,9,14,0,0};

	struct calendar actual;

	caljulian(testDate, &actual);

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}

void test_LeapYear() {
	u_long input = 3549902400UL; // 2012-06-28 20:00:00Z
	struct calendar expected = {2012, 179, 6, 28, 20, 0, 0};

	struct calendar actual;

	caljulian(input, &actual);

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}

void test_uLongBoundary() {
	u_long time = 4294967295UL; // 2036-02-07 6:28:15
	struct calendar expected = {2036,0,2,7,6,28,15};

	struct calendar actual;

	caljulian(time, &actual);

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}

void test_uLongWrapped() {
	u_long time = 0;
	struct calendar expected = {2036,0,2,7,6,28,16};

	struct calendar actual;

	caljulian(time, &actual);

	TEST_ASSERT_TRUE(IsEqual(expected, actual));
}
