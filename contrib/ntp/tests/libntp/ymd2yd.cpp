#include "libntptest.h"

class ymd2ydTest : public libntptest {
};

TEST_F(ymd2ydTest, NonLeapYearFebruary) {
	EXPECT_EQ(31+20, ymd2yd(2010,2,20)); //2010-02-20
}

TEST_F(ymd2ydTest, NonLeapYearJune) {
	int expected = 31+28+31+30+31+18; // 18 June non-leap year
	EXPECT_EQ(expected, ymd2yd(2011,6,18));
}

TEST_F(ymd2ydTest, LeapYearFebruary) {
	EXPECT_EQ(31+20, ymd2yd(2012,2,20)); //2012-02-20 (leap year)
}

TEST_F(ymd2ydTest, LeapYearDecember) {
	// 2012-12-31
	int expected = 31+29+31+30+31+30+31+31+30+31+30+31;
	EXPECT_EQ(expected, ymd2yd(2012,12,31));
}
