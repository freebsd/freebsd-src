#include "config.h"

#include "ntp_calendar.h"
#include "ntp_stdlib.h"

#include "unity.h"
#include "test-libntp.h"

void setUp(void);
void tearDown(void);
void test_CurrentYear(void);
void test_CurrentYearFuzz(void);
void test_TimeZoneOffset(void);
void test_WrongYearStart(void);
void test_PreviousYear(void);
void test_NextYear(void);
void test_NoReasonableConversion(void);
int isLE(u_int32 diff,u_int32 actual);
void test_AlwaysInLimit(void);


/* ---------------------------------------------------------------------
 * test fixture
 *
 * The clocktimeTest uses the NTP calendar feature to use a mockup
 * function for getting the current system time, so the tests are not
 * dependent on the actual system time.
 */

void
setUp()
{
	ntpcal_set_timefunc(timefunc);
	settime(2000, 1, 1, 0, 0, 0);

	return;
}

void
tearDown()
{
	ntpcal_set_timefunc(NULL);

	return;
}

/* ---------------------------------------------------------------------
 * test cases
 */

void
test_CurrentYear(void)
{
	/* Timestamp: 2010-06-24 12:50:00Z */
	const u_int32 timestamp = 3486372600UL;
	const u_int32 expected	= timestamp; /* exactly the same. */

	const int yday=175, hour=12, minute=50, second=0, tzoff=0;

	u_long yearstart = 0;
	u_int32 actual;

	TEST_ASSERT_TRUE(clocktime(yday, hour, minute, second, tzoff,
				   timestamp, &yearstart, &actual));
	TEST_ASSERT_EQUAL(expected, actual);

	return;
}

void
test_CurrentYearFuzz(void)
{
	/* 
	 * Timestamp (rec_ui) is: 2010-06-24 12:50:00
	 * Time sent into function is 12:00:00.
	 *
	 * Since the fuzz is rather small, we should get a NTP
	 * timestamp for the 12:00:00 time.
	 */

	const u_int32 timestamp = 3486372600UL; /* 2010-06-24 12:50:00Z */
	const u_int32 expected	= 3486369600UL; /* 2010-06-24 12:00:00Z */

	const int yday=175, hour=12, minute=0, second=0, tzoff=0;

	u_long yearstart=0;
	u_int32 actual;

	TEST_ASSERT_TRUE(clocktime(yday, hour, minute, second, tzoff,
				   timestamp, &yearstart, &actual));
	TEST_ASSERT_EQUAL(expected, actual);

	return;
}

void
test_TimeZoneOffset(void)
{
	/*
	 * Timestamp (rec_ui) is: 2010-06-24 12:00:00 +0800
	 * (which is 2010-06-24 04:00:00Z)
	 *
	 * Time sent into function is 04:00:00 +0800
	 */
	const u_int32 timestamp = 3486369600UL;
	const u_int32 expected	= timestamp;

	const int yday=175, hour=4, minute=0, second=0, tzoff=8;

	u_long yearstart=0;
	u_int32 actual;

	TEST_ASSERT_TRUE(clocktime(yday, hour, minute, second, tzoff, timestamp,
						  &yearstart, &actual));
	TEST_ASSERT_EQUAL(expected, actual);
}

void
test_WrongYearStart(void)
{
	/* 
	 * Timestamp (rec_ui) is: 2010-01-02 11:00:00Z
	 * Time sent into function is 11:00:00.
	 * Yearstart sent into function is the yearstart of 2009!
	 */
	const u_int32 timestamp = 3471418800UL;
	const u_int32 expected	= timestamp;

	const int yday=2, hour=11, minute=0, second=0, tzoff=0;

	u_long yearstart = 302024100UL; /* Yearstart of 2009. */
	u_int32 actual;

	TEST_ASSERT_TRUE(clocktime(yday, hour, minute, second, tzoff, timestamp,
						  &yearstart, &actual));
	TEST_ASSERT_EQUAL(expected, actual);
}

void
test_PreviousYear(void)
{
	/*
	 * Timestamp is: 2010-01-01 01:00:00Z
	 * Time sent into function is 23:00:00
	 * (which is meant to be 2009-12-31 23:00:00Z)
	 */
	const u_int32 timestamp = 3471296400UL;
	const u_int32 expected	= 3471289200UL;

	const int yday=365, hour=23, minute=0, second=0, tzoff=0;

	u_long yearstart = 0;
	u_int32 actual;

	TEST_ASSERT_TRUE(clocktime(yday, hour, minute, second, tzoff, timestamp,
						  &yearstart, &actual));
	TEST_ASSERT_EQUAL(expected, actual);
}

void
test_NextYear(void)
{
	/*
	 * Timestamp is: 2009-12-31 23:00:00Z
	 * Time sent into function is 01:00:00
	 * (which is meant to be 2010-01-01 01:00:00Z)
	 */
	const u_int32 timestamp = 3471289200UL;
	const u_int32 expected	= 3471296400UL;

	const int yday=1, hour=1, minute=0, second=0, tzoff=0;
	u_long yearstart = 0;
	u_int32 actual;

	TEST_ASSERT_TRUE(clocktime(yday, hour, minute, second, tzoff,
				   timestamp, &yearstart, &actual));
	TEST_ASSERT_EQUAL(expected, actual);

	return;
}

void
test_NoReasonableConversion(void)
{
	/* Timestamp is: 2010-01-02 11:00:00Z */
	const u_int32 timestamp = 3471418800UL;

	const int yday=100, hour=12, minute=0, second=0, tzoff=0;
	u_long yearstart = 0;
	u_int32 actual;

	TEST_ASSERT_FALSE(clocktime(yday, hour, minute, second, tzoff,
				    timestamp, &yearstart, &actual));

	return;
}


int/*BOOL*/
isLE(u_int32 diff,u_int32 actual)
{

	if (diff <= actual) {
		return TRUE;
	}
	else return FALSE;
}


void
test_AlwaysInLimit(void)
{
	/* Timestamp is: 2010-01-02 11:00:00Z */
	const u_int32 timestamp = 3471418800UL;
	const u_short prime_incs[] = { 127, 151, 163, 179 };
	int	cyc;
	int	yday;
	u_char	whichprime;
	u_short	ydayinc;
	int	hour;
	int	minute;
	u_long	yearstart;
	u_int32	actual;
	u_int32	diff;

	yearstart = 0;
	for (cyc = 0; cyc < 5; cyc++) {
		settime(1900 + cyc * 65, 1, 1, 0, 0, 0);
		for (yday = -26000; yday < 26000; yday += ydayinc) {
			whichprime = abs(yday) % COUNTOF(prime_incs);
			ydayinc = prime_incs[whichprime];
			for (hour = -204; hour < 204; hour += 2) {
				for (minute = -60; minute < 60; minute++) {
					clocktime(yday, hour, minute, 30, 0,
						  timestamp, &yearstart,
						  &actual);
					diff = actual - timestamp;
					if (diff >= 0x80000000UL)
						diff = ~diff + 1;
					TEST_ASSERT_TRUE(isLE(diff, (183u * SECSPERDAY)));
				}
			}
		}
	}
	return;
}
