#include "config.h"

#include "ntp_stdlib.h" /* test fail without this include, for some reason */
#include "ntp_calendar.h"
#include "ntp_unixtime.h"
#include "unity.h"

#include <string.h>

static int leapdays(int year);

void	setUp(void);
int	isGT(int first, int second);
int	leapdays(int year);
char *	CalendarFromCalToString(const struct calendar *cal);
char *	CalendarFromIsoToString(const struct isodate *iso); 
int	IsEqualCal(const struct calendar *expected, const struct calendar *actual);
int	IsEqualIso(const struct isodate *expected, const struct isodate *actual);
char *	DateFromCalToString(const struct calendar *cal);
char *	DateFromIsoToString(const struct isodate *iso);
int	IsEqualDateCal(const struct calendar *expected, const struct calendar *actual);
int	IsEqualDateIso(const struct isodate *expected, const struct isodate *actual);

void	test_DaySplitMerge(void);
void	test_SplitYearDays1(void);
void	test_SplitYearDays2(void);
void	test_RataDie1(void);
void	test_LeapYears1(void);
void	test_LeapYears2(void);
void	test_RoundTripDate(void);
void	test_RoundTripYearStart(void);
void	test_RoundTripMonthStart(void);
void	test_RoundTripWeekStart(void);
void	test_RoundTripDayStart(void);
void	test_IsoCalYearsToWeeks(void);
void	test_IsoCalWeeksToYearStart(void);
void	test_IsoCalWeeksToYearEnd(void);
void	test_DaySecToDate(void);

void	test_NtpToNtp(void);
void	test_NtpToTime(void);

void
setUp(void)
{
	init_lib();

	return;
}


/*
 * ---------------------------------------------------------------------
 * test support stuff
 * ---------------------------------------------------------------------
 */
int
isGT(int first, int second)
{
	if(first > second) {
		return TRUE;
	} else {
		return FALSE;
	}
}

int
leapdays(int year)
{
	if (year % 400 == 0)
		return 1;
	if (year % 100 == 0)
		return 0;
	if (year % 4 == 0)
		return 1;
	return 0;
}

char *
CalendarFromCalToString(
    const struct calendar *cal)
{
	char * str = malloc(sizeof (char) * 100);
	snprintf(str, 100, "%u-%02u-%02u (%u) %02u:%02u:%02u",
		 cal->year, (u_int)cal->month, (u_int)cal->monthday,
		 cal->yearday,
		 (u_int)cal->hour, (u_int)cal->minute, (u_int)cal->second);
	str[99] = '\0'; /* paranoia rulez! */
	return str;
}

char *
CalendarFromIsoToString(
	const struct isodate *iso)
{
	char * str = emalloc (sizeof (char) * 100);
	snprintf(str, 100, "%u-W%02u-%02u %02u:%02u:%02u",
		 iso->year, (u_int)iso->week, (u_int)iso->weekday,
		 (u_int)iso->hour, (u_int)iso->minute, (u_int)iso->second);
	str[99] = '\0'; /* paranoia rulez! */
	return str;
}

int
IsEqualCal(
	const struct calendar *expected,
	const struct calendar *actual)
{
	if (expected->year == actual->year &&
	    (!expected->yearday || expected->yearday == actual->yearday) &&
	    expected->month == actual->month &&
	    expected->monthday == actual->monthday &&
	    expected->hour == actual->hour &&
	    expected->minute == actual->minute &&
	    expected->second == actual->second) {
		return TRUE;
	} else {
		char *p_exp = CalendarFromCalToString(expected);
		char *p_act = CalendarFromCalToString(actual);

		printf("expected: %s but was %s", p_exp, p_act);

		free(p_exp);
		free(p_act);

		return FALSE;		  
	}
}

int
IsEqualIso(
	const struct isodate *expected,
	const struct isodate *actual)
{
	if (expected->year == actual->year &&
	    expected->week == actual->week &&
	    expected->weekday == actual->weekday &&
	    expected->hour == actual->hour &&
	    expected->minute == actual->minute &&
	    expected->second == actual->second) {
		return TRUE;
	} else {
		printf("expected: %s but was %s",
		       CalendarFromIsoToString(expected),
		       CalendarFromIsoToString(actual));
		return FALSE;	   
	}
}

char *
DateFromCalToString(
	const struct calendar *cal)
{

	char * str = emalloc (sizeof (char) * 100);
	snprintf(str, 100, "%u-%02u-%02u (%u)",
		 cal->year, (u_int)cal->month, (u_int)cal->monthday,
		 cal->yearday);
	str[99] = '\0'; /* paranoia rulez! */
	return str;
}

char *
DateFromIsoToString(
	const struct isodate *iso)
{

	char * str = emalloc (sizeof (char) * 100);
	snprintf(str, 100, "%u-W%02u-%02u",
		 iso->year, (u_int)iso->week, (u_int)iso->weekday);
	str[99] = '\0'; /* paranoia rulez! */
	return str;
}

int/*BOOL*/
IsEqualDateCal(
	const struct calendar *expected,
	const struct calendar *actual)
{
	if (expected->year == actual->year &&
	    (!expected->yearday || expected->yearday == actual->yearday) &&
	    expected->month == actual->month &&
	    expected->monthday == actual->monthday) {
		return TRUE;
	} else {
		printf("expected: %s but was %s",
		       DateFromCalToString(expected),
		       DateFromCalToString(actual));
		return FALSE;
	}
}

int/*BOOL*/
IsEqualDateIso(
	const struct isodate *expected,
	const struct isodate *actual)
{
	if (expected->year == actual->year &&
	    expected->week == actual->week &&
	    expected->weekday == actual->weekday) {
		return TRUE;
	} else {
		printf("expected: %s but was %s",
		       DateFromIsoToString(expected),
		       DateFromIsoToString(actual));
		return FALSE;	    
	}
}


/*
 * ---------------------------------------------------------------------
 * test cases
 * ---------------------------------------------------------------------
 */

/* days before month, with a full-year pad at the upper end */
static const u_short real_month_table[2][13] = {
	/* -*- table for regular years -*- */
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	/* -*- table for leap years -*- */
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

/* days in month, with one month wrap-around at both ends */
static const u_short real_month_days[2][14] = {
	/* -*- table for regular years -*- */
	{ 31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31 },
	/* -*- table for leap years -*- */
	{ 31, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31 }
};

/* test the day/sec join & split ops, making sure that 32bit
 * intermediate results would definitely overflow and the hi DWORD of
 * the 'vint64' is definitely needed.
 */
void
test_DaySplitMerge(void)
{
	int32 day,sec;

	for (day = -1000000; day <= 1000000; day += 100) {
		for (sec = -100000; sec <= 186400; sec += 10000) {
			vint64		 merge;
			ntpcal_split split;
			int32		 eday;
			int32		 esec;

			merge = ntpcal_dayjoin(day, sec);
			split = ntpcal_daysplit(&merge);
			eday  = day;
			esec  = sec;

			while (esec >= 86400) {
				eday += 1;
				esec -= 86400;
			}
			while (esec < 0) {
				eday -= 1;
				esec += 86400;
			}

			TEST_ASSERT_EQUAL(eday, split.hi);
			TEST_ASSERT_EQUAL(esec, split.lo);
		}
	}

	return;
}

void
test_SplitYearDays1(void)
{
	int32 eyd;

	for (eyd = -1; eyd <= 365; eyd++) {
		ntpcal_split split = ntpcal_split_yeardays(eyd, 0);
		if (split.lo >= 0 && split.hi >= 0) {
			TEST_ASSERT_TRUE(isGT(12,split.hi));
			TEST_ASSERT_TRUE(isGT(real_month_days[0][split.hi+1], split.lo));
			int32 tyd = real_month_table[0][split.hi] + split.lo;
			TEST_ASSERT_EQUAL(eyd, tyd);
		} else
			TEST_ASSERT_TRUE(eyd < 0 || eyd > 364);
	}

	return;
}

void
test_SplitYearDays2(void)
{
	int32 eyd;

	for (eyd = -1; eyd <= 366; eyd++) {
		ntpcal_split split = ntpcal_split_yeardays(eyd, 1);
		if (split.lo >= 0 && split.hi >= 0) {
			/* basic checks do not work on compunds :( */
			/* would like: TEST_ASSERT_TRUE(12 > split.hi); */
			TEST_ASSERT_TRUE(isGT(12,split.hi));
			TEST_ASSERT_TRUE(isGT(real_month_days[1][split.hi+1], split.lo));
			int32 tyd = real_month_table[1][split.hi] + split.lo;
			TEST_ASSERT_EQUAL(eyd, tyd);
		} else
			TEST_ASSERT_TRUE(eyd < 0 || eyd > 365);
		}

	return;
}

void
test_RataDie1(void)
{
	int32	 testDate = 1; /* 0001-01-01 (proleptic date) */
	struct calendar expected = { 1, 1, 1, 1 };
	struct calendar actual;

	ntpcal_rd_to_date(&actual, testDate);
	TEST_ASSERT_TRUE(IsEqualDateCal(&expected, &actual));

	return;
}

/* check last day of february for first 10000 years */
void
test_LeapYears1(void)
{
	struct calendar dateIn, dateOut;

	for (dateIn.year = 1; dateIn.year < 10000; ++dateIn.year) {
		dateIn.month	= 2;
		dateIn.monthday = 28 + leapdays(dateIn.year);
		dateIn.yearday	= 31 + dateIn.monthday;

		ntpcal_rd_to_date(&dateOut, ntpcal_date_to_rd(&dateIn));

		TEST_ASSERT_TRUE(IsEqualDateCal(&dateIn, &dateOut));
	}

	return;
}

/* check first day of march for first 10000 years */
void
test_LeapYears2(void)
{
	struct calendar dateIn, dateOut;

	for (dateIn.year = 1; dateIn.year < 10000; ++dateIn.year) {
		dateIn.month	= 3;
		dateIn.monthday = 1;
		dateIn.yearday	= 60 + leapdays(dateIn.year);

		ntpcal_rd_to_date(&dateOut, ntpcal_date_to_rd(&dateIn));
		TEST_ASSERT_TRUE(IsEqualDateCal(&dateIn, &dateOut));
	}

	return;
}

/* Full roundtrip from 1601-01-01 to 2400-12-31
 * checks sequence of rata die numbers and validates date output
 * (since the input is all nominal days of the calendar in that range
 * and the result of the inverse calculation must match the input no
 * invalid output can occur.)
 */
void
test_RoundTripDate(void)
{
	struct calendar truDate, expDate = { 1600, 0, 12, 31 };;
	int	 leaps;
	int32	 truRdn, expRdn	= ntpcal_date_to_rd(&expDate);

	while (expDate.year < 2400) {
		expDate.year++;
		expDate.month	= 0;
		expDate.yearday = 0;
		leaps = leapdays(expDate.year);
		while (expDate.month < 12) {
			expDate.month++;
			expDate.monthday = 0;
			while (expDate.monthday < real_month_days[leaps][expDate.month]) {
				expDate.monthday++;
				expDate.yearday++;
				expRdn++;

				truRdn = ntpcal_date_to_rd(&expDate);
				TEST_ASSERT_EQUAL(expRdn, truRdn);

				ntpcal_rd_to_date(&truDate, truRdn);
				TEST_ASSERT_TRUE(IsEqualDateCal(&expDate, &truDate));
			}
		}
	}

	return;
}

/* Roundtrip testing on calyearstart */
void
test_RoundTripYearStart(void)
{
	static const time_t pivot = 0;
	u_int32 ntp, expys, truys;
	struct calendar date;

	for (ntp = 0; ntp < 0xFFFFFFFFu - 30000000u; ntp += 30000000u) {
		truys = calyearstart(ntp, &pivot);
		ntpcal_ntp_to_date(&date, ntp, &pivot);
		date.month = date.monthday = 1;
		date.hour = date.minute = date.second = 0;
		expys = ntpcal_date_to_ntp(&date);
		TEST_ASSERT_EQUAL(expys, truys);
	}

	return;
}

/* Roundtrip testing on calmonthstart */
void
test_RoundTripMonthStart(void)
{
	static const time_t pivot = 0;
	u_int32 ntp, expms, trums;
	struct calendar date;

	for (ntp = 0; ntp < 0xFFFFFFFFu - 2000000u; ntp += 2000000u) {
		trums = calmonthstart(ntp, &pivot);
		ntpcal_ntp_to_date(&date, ntp, &pivot);
		date.monthday = 1;
		date.hour = date.minute = date.second = 0;
		expms = ntpcal_date_to_ntp(&date);
		TEST_ASSERT_EQUAL(expms, trums);
	}

	return;
}

/* Roundtrip testing on calweekstart */
void
test_RoundTripWeekStart(void)
{
	static const time_t pivot = 0;
	u_int32 ntp, expws, truws;
	struct isodate date;

	for (ntp = 0; ntp < 0xFFFFFFFFu - 600000u; ntp += 600000u) {
		truws = calweekstart(ntp, &pivot);
		isocal_ntp_to_date(&date, ntp, &pivot);
		date.hour = date.minute = date.second = 0;
		date.weekday = 1;
		expws = isocal_date_to_ntp(&date);
		TEST_ASSERT_EQUAL(expws, truws);
	}

	return;
}

/* Roundtrip testing on caldaystart */
void
test_RoundTripDayStart(void)
{
	static const time_t pivot = 0;
	u_int32 ntp, expds, truds;
	struct calendar date;

	for (ntp = 0; ntp < 0xFFFFFFFFu - 80000u; ntp += 80000u) {
		truds = caldaystart(ntp, &pivot);
		ntpcal_ntp_to_date(&date, ntp, &pivot);
		date.hour = date.minute = date.second = 0;
		expds = ntpcal_date_to_ntp(&date);
		TEST_ASSERT_EQUAL(expds, truds);
	}

	return;
}

/* ---------------------------------------------------------------------
 * ISO8601 week calendar internals
 *
 * The ISO8601 week calendar implementation is simple in the terms of
 * the math involved, but the implementation of the calculations must
 * take care of a few things like overflow, floor division, and sign
 * corrections.
 *
 * Most of the functions are straight forward, but converting from years
 * to weeks and from weeks to years warrants some extra tests. These use
 * an independent reference implementation of the conversion from years
 * to weeks.
 * ---------------------------------------------------------------------
 */

/* helper / reference implementation for the first week of year in the
 * ISO8601 week calendar. This is based on the reference definition of
 * the ISO week calendar start: The Monday closest to January,1st of the
 * corresponding year in the Gregorian calendar.
 */
static int32_t
refimpl_WeeksInIsoYears(
	int32_t years)
{
	int32_t days, weeks;

	days = ntpcal_weekday_close(
		ntpcal_days_in_years(years) + 1,
		CAL_MONDAY) - 1;
	/* the weekday functions operate on RDN, while we want elapsed
	 * units here -- we have to add / sub 1 in the midlle / at the
	 * end of the operation that gets us the first day of the ISO
	 * week calendar day.
	 */
	weeks = days / 7;
	days  = days % 7;
	TEST_ASSERT_EQUAL(0, days); /* paranoia check... */

	return weeks;
}

/* The next tests loop over 5000yrs, but should still be very fast. If
 * they are not, the calendar needs a better implementation...
 */
void
test_IsoCalYearsToWeeks(void)
{
	int32_t years;
	int32_t wref, wcal;

	for (years = -1000; years < 4000; ++years) {
		/* get number of weeks before years (reference) */
		wref = refimpl_WeeksInIsoYears(years);
		/* get number of weeks before years (object-under-test) */
		wcal = isocal_weeks_in_years(years);
		TEST_ASSERT_EQUAL(wref, wcal);
	}

	return;
}

void
test_IsoCalWeeksToYearStart(void)
{
	int32_t years;
	int32_t wref;
	ntpcal_split ysplit;

	for (years = -1000; years < 4000; ++years) {
		/* get number of weeks before years (reference) */
		wref = refimpl_WeeksInIsoYears(years);
		/* reverse split */
		ysplit = isocal_split_eraweeks(wref);
		/* check invariants: same year, week 0 */
		TEST_ASSERT_EQUAL(years, ysplit.hi);
		TEST_ASSERT_EQUAL(0, ysplit.lo);
	}

	return;
}

void
test_IsoCalWeeksToYearEnd(void)
{
	int32_t years;
	int32_t wref;
	ntpcal_split ysplit;

	for (years = -1000; years < 4000; ++years) {
		/* get last week of previous year */
		wref = refimpl_WeeksInIsoYears(years) - 1;
		/* reverse split */
		ysplit = isocal_split_eraweeks(wref);
		/* check invariants: previous year, week 51 or 52 */
		TEST_ASSERT_EQUAL(years-1, ysplit.hi);
		TEST_ASSERT(ysplit.lo == 51 || ysplit.lo == 52);
	}

	return;
}

void
test_DaySecToDate(void)
{
	struct calendar cal;
	int32_t days;

	days = ntpcal_daysec_to_date(&cal, -86400);
	TEST_ASSERT_MESSAGE((days==-1 && cal.hour==0 && cal.minute==0 && cal.second==0),
		"failed for -86400");

	days = ntpcal_daysec_to_date(&cal, -86399);
	TEST_ASSERT_MESSAGE((days==-1 && cal.hour==0 && cal.minute==0 && cal.second==1),
		"failed for -86399");

	days = ntpcal_daysec_to_date(&cal, -1);
	TEST_ASSERT_MESSAGE((days==-1 && cal.hour==23 && cal.minute==59 && cal.second==59),
		"failed for -1");

	days = ntpcal_daysec_to_date(&cal, 0);
	TEST_ASSERT_MESSAGE((days==0 && cal.hour==0 && cal.minute==0 && cal.second==0),
		"failed for 0");

	days = ntpcal_daysec_to_date(&cal, 1);
	TEST_ASSERT_MESSAGE((days==0 && cal.hour==0 && cal.minute==0 && cal.second==1),
		"failed for 1");

	days = ntpcal_daysec_to_date(&cal, 86399);
	TEST_ASSERT_MESSAGE((days==0 && cal.hour==23 && cal.minute==59 && cal.second==59),
		"failed for 86399");

	days = ntpcal_daysec_to_date(&cal, 86400);
	TEST_ASSERT_MESSAGE((days==1 && cal.hour==0 && cal.minute==0 && cal.second==0),
		"failed for 86400");

	return;
}

/* --------------------------------------------------------------------
 * unfolding of (truncated) NTP time stamps to full 64bit values.
 *
 * Note: These tests need a 64bit time_t to be useful.
 */

void
test_NtpToNtp(void)
{
#   if SIZEOF_TIME_T <= 4
	
	TEST_IGNORE_MESSAGE("test only useful for sizeof(time_t) > 4, skipped");

#   else
	
	static const uint32_t ntp_vals[6] = {
		UINT32_C(0x00000000),
		UINT32_C(0x00000001),
		UINT32_C(0x7FFFFFFF),
		UINT32_C(0x80000000),
		UINT32_C(0x80000001),
		UINT32_C(0xFFFFFFFF)
	};

	static char	lbuf[128];
	vint64		hold;
	time_t		pivot, texp, diff;
	int		loops, iloop;
	
	pivot = 0;
	for (loops = 0; loops < 16; ++loops) {
		for (iloop = 0; iloop < 6; ++iloop) {
			hold = ntpcal_ntp_to_ntp(
				ntp_vals[iloop], &pivot);
			texp = vint64_to_time(&hold);

			/* constraint 1: texp must be in the
			 * (right-open) intervall [p-(2^31), p+(2^31)[,
			 * but the pivot 'p' must be taken in full NTP
			 * time scale!
			 */
			diff = texp - (pivot + JAN_1970);
			snprintf(lbuf, sizeof(lbuf),
				 "bounds check: piv=%lld exp=%lld dif=%lld",
				 (long long)pivot,
				 (long long)texp,
				 (long long)diff);
			TEST_ASSERT_MESSAGE((diff >= INT32_MIN) && (diff <= INT32_MAX),
					    lbuf);

			/* constraint 2: low word must be equal to
			 * input
			 */
			snprintf(lbuf, sizeof(lbuf),
				 "low check: ntp(in)=$%08lu ntp(out[0:31])=$%08lu",
				 (unsigned long)ntp_vals[iloop],
				 (unsigned long)hold.D_s.lo);
			TEST_ASSERT_EQUAL_MESSAGE(ntp_vals[iloop], hold.D_s.lo, lbuf);
		}
		pivot += 0x20000000;
	}
#   endif
}

void
test_NtpToTime(void)
{
#   if SIZEOF_TIME_T <= 4
	
	TEST_IGNORE_MESSAGE("test only useful for sizeof(time_t) > 4, skipped");
	
#   else
	
	static const uint32_t ntp_vals[6] = {
		UINT32_C(0x00000000),
		UINT32_C(0x00000001),
		UINT32_C(0x7FFFFFFF),
		UINT32_C(0x80000000),
		UINT32_C(0x80000001),
		UINT32_C(0xFFFFFFFF)
	};

	static char	lbuf[128];
	vint64		hold;
	time_t		pivot, texp, diff;
	uint32_t	back;
	int		loops, iloop;
	
	pivot = 0;
	for (loops = 0; loops < 16; ++loops) {
		for (iloop = 0; iloop < 6; ++iloop) {
			hold = ntpcal_ntp_to_time(
				ntp_vals[iloop], &pivot);
			texp = vint64_to_time(&hold);

			/* constraint 1: texp must be in the
			 * (right-open) intervall [p-(2^31), p+(2^31)[
			 */
			diff = texp - pivot;
			snprintf(lbuf, sizeof(lbuf),
				 "bounds check: piv=%lld exp=%lld dif=%lld",
				 (long long)pivot,
				 (long long)texp,
				 (long long)diff);
			TEST_ASSERT_MESSAGE((diff >= INT32_MIN) && (diff <= INT32_MAX),
					    lbuf);

			/* constraint 2: conversion from full time back
			 * to truncated NTP time must yield same result
			 * as input.
			*/
			back = (uint32_t)texp + JAN_1970;
			snprintf(lbuf, sizeof(lbuf),
				 "modulo check: ntp(in)=$%08lu ntp(out)=$%08lu",
				 (unsigned long)ntp_vals[iloop],
				 (unsigned long)back);
			TEST_ASSERT_EQUAL_MESSAGE(ntp_vals[iloop], back, lbuf);
		}
		pivot += 0x20000000;
	}
#   endif
}
