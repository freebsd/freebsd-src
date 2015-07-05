#include "g_libntptest.h"

extern "C" {
#include "ntp_calendar.h"
}

#include <string>
#include <sstream>

class calendarTest : public libntptest {
protected:
	static int leapdays(int year);

	std::string CalendarToString(const calendar &cal);
	std::string CalendarToString(const isodate &iso);
	::testing::AssertionResult IsEqual(const calendar &expected, const calendar &actual);
	::testing::AssertionResult IsEqual(const isodate &expected, const isodate &actual);

	std::string DateToString(const calendar &cal);
	std::string DateToString(const isodate &iso);
	::testing::AssertionResult IsEqualDate(const calendar &expected, const calendar &actual);
	::testing::AssertionResult IsEqualDate(const isodate &expected, const isodate &actual);
};


// ---------------------------------------------------------------------
// test support stuff
// ---------------------------------------------------------------------
int
calendarTest::leapdays(int year)
{
	if (year % 400 == 0)
		return 1;
	if (year % 100 == 0)
		return 0;
	if (year % 4 == 0)
		return 1;
	return 0;
}

std::string 
calendarTest::CalendarToString(const calendar &cal) {
	std::ostringstream ss;
	ss << cal.year << "-" << (u_int)cal.month << "-" << (u_int)cal.monthday
	   << " (" << cal.yearday << ") " << (u_int)cal.hour << ":"
	   << (u_int)cal.minute << ":" << (u_int)cal.second;
	return ss.str();
}

std::string
calendarTest:: CalendarToString(const isodate &iso) {
	std::ostringstream ss;
	ss << iso.year << "-" << (u_int)iso.week << "-" << (u_int)iso.weekday
	   << (u_int)iso.hour << ":" << (u_int)iso.minute << ":" << (u_int)iso.second;
	return ss.str();
}

::testing::AssertionResult
calendarTest:: IsEqual(const calendar &expected, const calendar &actual) {
	if (expected.year == actual.year &&
	    (!expected.yearday || expected.yearday == actual.yearday) &&
	    expected.month == actual.month &&
	    expected.monthday == actual.monthday &&
	    expected.hour == actual.hour &&
	    expected.minute == actual.minute &&
	    expected.second == actual.second) {
		return ::testing::AssertionSuccess();
	} else {
		return ::testing::AssertionFailure()
		    << "expected: " << CalendarToString(expected) << " but was "
		    << CalendarToString(actual);
	}
}

::testing::AssertionResult
calendarTest:: IsEqual(const isodate &expected, const isodate &actual) {
	if (expected.year == actual.year &&
	    expected.week == actual.week &&
	    expected.weekday == actual.weekday &&
	    expected.hour == actual.hour &&
	    expected.minute == actual.minute &&
	    expected.second == actual.second) {
		return ::testing::AssertionSuccess();
	} else {
		return ::testing::AssertionFailure()
		    << "expected: " << CalendarToString(expected) << " but was "
		    << CalendarToString(actual);
	}
}

std::string
calendarTest:: DateToString(const calendar &cal) {
	std::ostringstream ss;
	ss << cal.year << "-" << (u_int)cal.month << "-" << (u_int)cal.monthday
	   << " (" << cal.yearday << ")";
	return ss.str();
}

std::string
calendarTest:: DateToString(const isodate &iso) {
	std::ostringstream ss;
	ss << iso.year << "-" << (u_int)iso.week << "-" << (u_int)iso.weekday;
	return ss.str();
}

::testing::AssertionResult
calendarTest:: IsEqualDate(const calendar &expected, const calendar &actual) {
	if (expected.year == actual.year &&
	    (!expected.yearday || expected.yearday == actual.yearday) &&
	    expected.month == actual.month &&
	    expected.monthday == actual.monthday) {
		return ::testing::AssertionSuccess();
	} else {
		return ::testing::AssertionFailure()
		    << "expected: " << DateToString(expected) << " but was "
		    << DateToString(actual);
	}
}

::testing::AssertionResult
calendarTest:: IsEqualDate(const isodate &expected, const isodate &actual) {
	if (expected.year == actual.year &&
	    expected.week == actual.week &&
	    expected.weekday == actual.weekday) {
		return ::testing::AssertionSuccess();
	} else {
		return ::testing::AssertionFailure()
		    << "expected: " << DateToString(expected) << " but was "
		    << DateToString(actual);
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
TEST_F(calendarTest, DaySplitMerge) {
	for (int32 day = -1000000; day <= 1000000; day += 100) {
		for (int32 sec = -100000; sec <= 186400; sec += 10000) {
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

			EXPECT_EQ(eday, split.hi);
			EXPECT_EQ(esec, split.lo);
		}
	}
}

TEST_F(calendarTest, SplitYearDays1) {
	for (int32 eyd = -1; eyd <= 365; eyd++) {
		ntpcal_split split = ntpcal_split_yeardays(eyd, 0);
		if (split.lo >= 0 && split.hi >= 0) {
			EXPECT_GT(12, split.hi);
			EXPECT_GT(real_month_days[0][split.hi+1], split.lo);
			int32 tyd = real_month_table[0][split.hi] + split.lo;
			EXPECT_EQ(eyd, tyd);
		} else
			EXPECT_TRUE(eyd < 0 || eyd > 364);
	}
}
		
TEST_F(calendarTest, SplitYearDays2) {
	for (int32 eyd = -1; eyd <= 366; eyd++) {
		ntpcal_split split = ntpcal_split_yeardays(eyd, 1);
		if (split.lo >= 0 && split.hi >= 0) {
			EXPECT_GT(12, split.hi);
			EXPECT_GT(real_month_days[1][split.hi+1], split.lo);
			int32 tyd = real_month_table[1][split.hi] + split.lo;
			EXPECT_EQ(eyd, tyd);
		} else
			EXPECT_TRUE(eyd < 0 || eyd > 365);
		}
}
		
TEST_F(calendarTest, RataDie1) {
	int32	 testDate = 1; // 0001-01-01 (proleptic date)
	calendar expected = { 1, 1, 1, 1 };
	calendar actual;

	ntpcal_rd_to_date(&actual, testDate);
	EXPECT_TRUE(IsEqualDate(expected, actual));
}

// check last day of february for first 10000 years
TEST_F(calendarTest, LeapYears1) {
	calendar dateIn, dateOut;

	for (dateIn.year = 1; dateIn.year < 10000; ++dateIn.year) {
		dateIn.month	= 2;
		dateIn.monthday = 28 + leapdays(dateIn.year);
		dateIn.yearday	= 31 + dateIn.monthday;

		ntpcal_rd_to_date(&dateOut, ntpcal_date_to_rd(&dateIn));

		EXPECT_TRUE(IsEqualDate(dateIn, dateOut));
	}
}

// check first day of march for first 10000 years
TEST_F(calendarTest, LeapYears2) {
	calendar dateIn, dateOut;

	for (dateIn.year = 1; dateIn.year < 10000; ++dateIn.year) {
		dateIn.month	= 3;
		dateIn.monthday = 1;
		dateIn.yearday	= 60 + leapdays(dateIn.year);

		ntpcal_rd_to_date(&dateOut, ntpcal_date_to_rd(&dateIn));
		EXPECT_TRUE(IsEqualDate(dateIn, dateOut));
	}
}

// Full roundtrip for 1601-01-01 to 2400-12-31
// checks sequence of rata die numbers and validates date output
// (since the input is all nominal days of the calendar in that range
// and the result of the inverse calculation must match the input no
// invalid output can occur.)
TEST_F(calendarTest, RoundTripDate) {
	calendar truDate, expDate = { 1600, 0, 12, 31 };;
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
				EXPECT_EQ(expRdn, truRdn);

				ntpcal_rd_to_date(&truDate, truRdn);
				EXPECT_TRUE(IsEqualDate(expDate, truDate));
			}
		}
	}
}

// Roundtrip testing on calyearstart
TEST_F(calendarTest, RoundTripYearStart) {
	static const time_t pivot = 0;
	u_int32 ntp, expys, truys;
	calendar date;

	for (ntp = 0; ntp < 0xFFFFFFFFu - 30000000u; ntp += 30000000u) {
		truys = calyearstart(ntp, &pivot);
		ntpcal_ntp_to_date(&date, ntp, &pivot);
		date.month = date.monthday = 1;
		date.hour = date.minute = date.second = 0;
		expys = ntpcal_date_to_ntp(&date);
		EXPECT_EQ(expys, truys);
	}
}	

// Roundtrip testing on calymonthstart
TEST_F(calendarTest, RoundTripMonthStart) {
	static const time_t pivot = 0;
	u_int32 ntp, expms, trums;
	calendar date;

	for (ntp = 0; ntp < 0xFFFFFFFFu - 2000000u; ntp += 2000000u) {
		trums = calmonthstart(ntp, &pivot);
		ntpcal_ntp_to_date(&date, ntp, &pivot);
		date.monthday = 1;
		date.hour = date.minute = date.second = 0;
		expms = ntpcal_date_to_ntp(&date);
		EXPECT_EQ(expms, trums);
	}
}	

// Roundtrip testing on calweekstart
TEST_F(calendarTest, RoundTripWeekStart) {
	static const time_t pivot = 0;
	u_int32 ntp, expws, truws;
	isodate date;

	for (ntp = 0; ntp < 0xFFFFFFFFu - 600000u; ntp += 600000u) {
		truws = calweekstart(ntp, &pivot);
		isocal_ntp_to_date(&date, ntp, &pivot);
		date.hour = date.minute = date.second = 0;
		date.weekday = 1;
		expws = isocal_date_to_ntp(&date);
		EXPECT_EQ(expws, truws);
	}
}	

// Roundtrip testing on caldaystart
TEST_F(calendarTest, RoundTripDayStart) {
	static const time_t pivot = 0;
	u_int32 ntp, expds, truds;
	calendar date;

	for (ntp = 0; ntp < 0xFFFFFFFFu - 80000u; ntp += 80000u) {
		truds = caldaystart(ntp, &pivot);
		ntpcal_ntp_to_date(&date, ntp, &pivot);
		date.hour = date.minute = date.second = 0;
		expds = ntpcal_date_to_ntp(&date);
		EXPECT_EQ(expds, truds);
	}
}	

