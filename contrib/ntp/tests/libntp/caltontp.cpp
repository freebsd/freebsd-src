#include "libntptest.h"

extern "C" {
#include "ntp_calendar.h"
}

class caltontpTest : public libntptest {
};

TEST_F(caltontpTest, DateGivenMonthDay) {
	// 2010-06-24 12:50:00
	calendar input = {2010, 0, 6, 24, 12, 50, 0};

	u_long expected = 3486372600UL; // This is the timestamp above.

	EXPECT_EQ(expected, caltontp(&input));
}

TEST_F(caltontpTest, DateGivenYearDay) {
	// 2010-06-24 12:50:00
	// This is the 175th day of 2010.
	calendar input = {2010, 175, 0, 0, 12, 50, 0};

	u_long expected = 3486372600UL; // This is the timestamp above.

	EXPECT_EQ(expected, caltontp(&input));
}

TEST_F(caltontpTest, DateLeapYear) {
	// 2012-06-24 12:00:00
	// This is the 176th day of 2012 (since 2012 is a leap year).
	calendar inputYd = {2012, 176, 0, 0, 12, 00, 00};
	calendar inputMd = {2012, 0, 6, 24, 12, 00, 00};

	u_long expected = 3549528000UL;

	EXPECT_EQ(expected, caltontp(&inputYd));
	EXPECT_EQ(expected, caltontp(&inputMd));
}

TEST_F(caltontpTest, WraparoundDateIn2036) {
	// 2036-02-07 06:28:16
	// This is (one) wrapping boundary where we go from ULONG_MAX to 0.
	calendar input = {2036, 0, 2, 7, 6, 28, 16};

	u_long expected = 0UL;

	EXPECT_EQ(expected, caltontp(&input));
}
