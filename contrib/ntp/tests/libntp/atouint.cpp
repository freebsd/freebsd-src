#include "libntptest.h"

class atouintTest : public libntptest {
};

TEST_F(atouintTest, RegularPositive) {
	const char *str = "305";
	u_long actual;

	ASSERT_TRUE(atouint(str, &actual));
	EXPECT_EQ(305, actual);
}

TEST_F(atouintTest, PositiveOverflowBoundary) {
	const char *str = "4294967296";
	u_long actual;

	ASSERT_FALSE(atouint(str, &actual));
}

TEST_F(atouintTest, PositiveOverflowBig) {
	const char *str = "8000000000";
	u_long actual;

	ASSERT_FALSE(atouint(str, &actual));
}

TEST_F(atouintTest, Negative) {
	const char *str = "-1";
	u_long actual;

	ASSERT_FALSE(atouint(str, &actual));
}

TEST_F(atouintTest, IllegalChar) {
	const char *str = "50c3";
	u_long actual;

	ASSERT_FALSE(atouint(str, &actual));
}
