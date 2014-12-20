#include "libntptest.h"

class atointTest : public libntptest {
};

TEST_F(atointTest, RegularPositive) {
	const char *str = "17";
	long val;

	ASSERT_TRUE(atoint(str, &val));
	EXPECT_EQ(17, val);
}

TEST_F(atointTest, RegularNegative) {
	const char *str = "-20";
	long val;

	ASSERT_TRUE(atoint(str, &val));
	EXPECT_EQ(-20, val);
}

TEST_F(atointTest, PositiveOverflowBoundary) {
	const char *str = "2147483648";
	long val;

	EXPECT_FALSE(atoint(str, &val));
}

TEST_F(atointTest, NegativeOverflowBoundary) {
	const char *str = "-2147483649";
	long val;

	EXPECT_FALSE(atoint(str, &val));
}

TEST_F(atointTest, PositiveOverflowBig) {
	const char *str = "2300000000";
	long val;

	EXPECT_FALSE(atoint(str, &val));
}

TEST_F(atointTest, IllegalCharacter) {
	const char *str = "4500l";
	long val;

	EXPECT_FALSE(atoint(str, &val));
}
