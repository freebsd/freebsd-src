#include "config.h"

#include "ntp_stdlib.h" //test fail without this include, for some reason
#include "ntp_calendar.h"
#include "unity.h"

//#include "test-libntp.h"


#include <string.h>
//#include <sstream>

static int leapdays(int year);

char * CalendarFromCalToString(const struct calendar cal); //&
char * CalendarFromIsoToString(const struct isodate iso); //&

//tehnically, booleans
int IsEqualCal(const struct calendar expected, const struct calendar actual);  //&&
int IsEqualIso(const struct isodate expected, const struct isodate actual); //&&

char * DateFromCalToStringCal(const struct calendar cal); //&
char * DateFromIsoToStringIso(const struct isodate iso); //&

//tehnically, booleans
int sEqualDateCal(const struct calendar expected, const struct calendar actual); //&&
int IsEqualDateIso(const struct isodate expected, const struct isodate actual); //&&



// ---------------------------------------------------------------------
// test support stuff
// ---------------------------------------------------------------------

//function which, in combination with TEST_ASSERT_TRUE replaces google test framework's EXPECT_GT(a,b); -> GT means Greather Than
//boolean
int isGT(int first,int second){
	if(first > second){
	
	return TRUE;
	}

	else return FALSE;
}


int leapdays(int year)
{
	if (year % 400 == 0)
		return 1;
	if (year % 100 == 0)
		return 0;
	if (year % 4 == 0)
		return 1;
	return 0;
}

char * CalendarFromCalToString(const struct calendar cal) { //&
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
	sprintf(buffer, "%u", cal.yearday);
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

char * CalendarFromIsoToString(const struct isodate iso) { //&

	char * ss = malloc (sizeof (char) * 100);
	
	char buffer[100] ="";
	sprintf(buffer, "%u", iso.year);
	strcat(ss,buffer);
	strcat(ss,"-");
	sprintf(buffer, "%u", (u_int)iso.week);
	strcat(ss,buffer);
	strcat(ss,"-");
	sprintf(buffer, "%u", (u_int)iso.weekday);
	strcat(ss,buffer);
	sprintf(buffer, "%u", (u_int)iso.hour);
	strcat(ss,buffer);
	strcat(ss,":");
	sprintf(buffer, "%u", (u_int)iso.minute);
	strcat(ss,buffer);
	strcat(ss,":");
	sprintf(buffer, "%u", (u_int)iso.second);
	strcat(ss,buffer);
	//ss << iso.year << "-" << (u_int)iso.week << "-" << (u_int)iso.weekday << (u_int)iso.hour << ":" << (u_int)iso.minute << ":" << (u_int)iso.second;	
	return ss;	

}

int IsEqualCal(const struct calendar expected, const struct calendar actual) { //&&
	if (expected.year == actual.year &&
	    (!expected.yearday || expected.yearday == actual.yearday) &&
	    expected.month == actual.month &&
	    expected.monthday == actual.monthday &&
	    expected.hour == actual.hour &&
	    expected.minute == actual.minute &&
	    expected.second == actual.second) {
		return TRUE;
	} else {
		printf("expected: %s but was %s", CalendarFromCalToString(expected) , CalendarFromCalToString(actual));
		return FALSE;		  
	}
}

int IsEqualIso(const struct isodate expected, const struct isodate actual) { //&&
	if (expected.year == actual.year &&
	    expected.week == actual.week &&
	    expected.weekday == actual.weekday &&
	    expected.hour == actual.hour &&
	    expected.minute == actual.minute &&
	    expected.second == actual.second) {
		return TRUE;
	} else {
		printf("expected: %s but was %s", CalendarFromIsoToString(expected) , CalendarFromIsoToString(actual));
		return FALSE;	   
	}
}

char * DateFromCalToString(const struct calendar cal) { //&

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
	sprintf(buffer, "%u", cal.yearday);
	strcat(ss,buffer);
	strcat(ss,")");
	
	return ss;
	//ss << cal.year << "-" << (u_int)cal.month << "-" << (u_int)cal.monthday << " (" << cal.yearday << ")";
}

char * DateFromIsoToString(const struct isodate iso) { //&

	char * ss = malloc (sizeof (char) * 100);
	
	char buffer[100] ="";
	sprintf(buffer, "%u", iso.year);
	strcat(ss,buffer);
	strcat(ss,"-");
	sprintf(buffer, "%u", (u_int)iso.week);
	strcat(ss,buffer);
	strcat(ss,"-");
	sprintf(buffer, "%u", (u_int)iso.weekday);
	strcat(ss,buffer);

	return ss;
	//ss << iso.year << "-" << (u_int)iso.week << "-" << (u_int)iso.weekday;
	
}

//boolean 
int IsEqualDateCal(const struct calendar expected, const struct calendar actual) { //&&
	if (expected.year == actual.year &&
	    (!expected.yearday || expected.yearday == actual.yearday) &&
	    expected.month == actual.month &&
	    expected.monthday == actual.monthday) {
		return TRUE;
	} else {
		printf("expected: %s but was %s", DateFromCalToString(expected) ,DateFromCalToString(actual));
		return FALSE;
	}
}

//boolean
int IsEqualDateIso(const struct isodate expected, const struct isodate actual) { //&&
	if (expected.year == actual.year &&
	    expected.week == actual.week &&
	    expected.weekday == actual.weekday) {
		return TRUE;
	} else {
		printf("expected: %s but was %s", DateFromIsoToString(expected) ,DateFromIsoToString(actual));
		return FALSE;	    
	}
}


// ---------------------------------------------------------------------
// test cases
// ---------------------------------------------------------------------
static const u_short real_month_table[2][13] = {
	/* -*- table for regular years -*- */
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	/* -*- table for leap years -*- */
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

// days in month, with one month wrap-around at both ends
static const u_short real_month_days[2][14] = {
	/* -*- table for regular years -*- */
	{ 31, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31 },
	/* -*- table for leap years -*- */
	{ 31, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31 }
};

// test the day/sec join & split ops, making sure that 32bit
// intermediate results would definitely overflow and the hi DWORD of
// the 'vint64' is definitely needed.
void test_DaySplitMerge() {
	int32 day,sec;
	for (day = -1000000; day <= 1000000; day += 100) {
		for (sec = -100000; sec <= 186400; sec += 10000) {
			vint64	     merge = ntpcal_dayjoin(day, sec);
			ntpcal_split split = ntpcal_daysplit(&merge);
			int32	     eday  = day;
			int32	     esec  = sec;

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
}

void test_SplitYearDays1() {
	int32 eyd;
	for (eyd = -1; eyd <= 365; eyd++) {
		ntpcal_split split = ntpcal_split_yeardays(eyd, 0);
		if (split.lo >= 0 && split.hi >= 0) {
			TEST_ASSERT_TRUE(isGT(12,split.hi));//EXPECT_GT(12, split.hi);
			TEST_ASSERT_TRUE(isGT(real_month_days[0][split.hi+1], split.lo));//EXPECT_GT(real_month_days[0][split.hi+1], split.lo);
			int32 tyd = real_month_table[0][split.hi] + split.lo;
			TEST_ASSERT_EQUAL(eyd, tyd);
		} else
			TEST_ASSERT_TRUE(eyd < 0 || eyd > 364);
	}
}
		
void test_SplitYearDays2() {
	int32 eyd;
	for (eyd = -1; eyd <= 366; eyd++) {
		ntpcal_split split = ntpcal_split_yeardays(eyd, 1);
		if (split.lo >= 0 && split.hi >= 0) {
			//TEST_ASSERT_TRUE(12 > split.hi); //simpler version, works for basic types, doesn't work for complex structs
			TEST_ASSERT_TRUE(isGT(12,split.hi));//EXPECT_GT(12, split.hi);
			TEST_ASSERT_TRUE(isGT(real_month_days[1][split.hi+1], split.lo));//EXPECT_GT(real_month_days[1][split.hi+1], split.lo);
			int32 tyd = real_month_table[1][split.hi] + split.lo;
			TEST_ASSERT_EQUAL(eyd, tyd);
		} else
			TEST_ASSERT_TRUE(eyd < 0 || eyd > 365);
		}
}
		
void test_RataDie1() {
	int32	 testDate = 1; // 0001-01-01 (proleptic date)
	struct calendar expected = { 1, 1, 1, 1 };
	struct calendar actual;

	ntpcal_rd_to_date(&actual, testDate);
	TEST_ASSERT_TRUE(IsEqualDateCal(expected, actual));
}

// check last day of february for first 10000 years
void test_LeapYears1() {
	struct calendar dateIn, dateOut;

	for (dateIn.year = 1; dateIn.year < 10000; ++dateIn.year) {
		dateIn.month	= 2;
		dateIn.monthday = 28 + leapdays(dateIn.year);
		dateIn.yearday	= 31 + dateIn.monthday;

		ntpcal_rd_to_date(&dateOut, ntpcal_date_to_rd(&dateIn));

		TEST_ASSERT_TRUE(IsEqualDateCal(dateIn, dateOut));
	}
}

// check first day of march for first 10000 years
void test_LeapYears2() {
	struct calendar dateIn, dateOut;

	for (dateIn.year = 1; dateIn.year < 10000; ++dateIn.year) {
		dateIn.month	= 3;
		dateIn.monthday = 1;
		dateIn.yearday	= 60 + leapdays(dateIn.year);

		ntpcal_rd_to_date(&dateOut, ntpcal_date_to_rd(&dateIn));
		TEST_ASSERT_TRUE(IsEqualDateCal(dateIn, dateOut));
	}
}

// Full roundtrip for 1601-01-01 to 2400-12-31
// checks sequence of rata die numbers and validates date output
// (since the input is all nominal days of the calendar in that range
// and the result of the inverse calculation must match the input no
// invalid output can occur.)
void test_RoundTripDate() {
	struct calendar truDate, expDate = { 1600, 0, 12, 31 };;
	int32	 truRdn, expRdn	= ntpcal_date_to_rd(&expDate);
	int	 leaps;

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
				TEST_ASSERT_TRUE(IsEqualDateCal(expDate, truDate));
			}
		}
	}
}

// Roundtrip testing on calyearstart
void test_RoundTripYearStart() {
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
}	

// Roundtrip testing on calymonthstart
void test_RoundTripMonthStart() {
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
}	

// Roundtrip testing on calweekstart
void test_RoundTripWeekStart() {
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
}	

// Roundtrip testing on caldaystart
void test_RoundTripDayStart() {
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
}	

