#include "libntptest.h"

class hextointTest : public libntptest {
};

TEST_F(hextointTest, SingleDigit) {
	const char *str = "a"; // 10 decimal
	u_long actual;

	ASSERT_TRUE(hextoint(str, &actual));
	EXPECT_EQ(10, actual);
}

TEST_F(hextointTest, MultipleDigits) {
	const char *str = "8F3"; // 2291 decimal
	u_long actual;

	ASSERT_TRUE(hextoint(str, &actual));
	EXPECT_EQ(2291, actual);
}

TEST_F(hextointTest, MaxUnsigned) {
	const char *str = "ffffffff"; // 4294967295 decimal
	u_long actual;

	ASSERT_TRUE(hextoint(str, &actual));
	EXPECT_EQ(4294967295UL, actual);
}

TEST_F(hextointTest, Overflow) {
	const char *str = "100000000"; // Overflow by 1
	u_long actual;

	ASSERT_FALSE(hextoint(str, &actual));
}

TEST_F(hextointTest, IllegalChar) {
	const char *str = "5gb"; // Illegal character g
	u_long actual;

	ASSERT_FALSE(hextoint(str, &actual));
}
