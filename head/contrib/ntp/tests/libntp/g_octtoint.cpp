#include "g_libntptest.h"

class octtointTest : public libntptest {
};

TEST_F(octtointTest, SingleDigit) {
	const char* str = "5";
	u_long actual;

	ASSERT_TRUE(octtoint(str, &actual));
	EXPECT_EQ(5, actual);
}

TEST_F(octtointTest, MultipleDigits) {
	const char* str = "271";
	u_long actual;

	ASSERT_TRUE(octtoint(str, &actual));
	EXPECT_EQ(185, actual);
}

TEST_F(octtointTest, Zero) {
	const char* str = "0";
	u_long actual;

	ASSERT_TRUE(octtoint(str, &actual));
	EXPECT_EQ(0, actual);
}

TEST_F(octtointTest, MaximumUnsigned32bit) {
	const char* str = "37777777777";
	u_long actual;

	ASSERT_TRUE(octtoint(str, &actual));
	EXPECT_EQ(4294967295UL, actual);
}

TEST_F(octtointTest, Overflow) {
	const char* str = "40000000000";
	u_long actual;

	ASSERT_FALSE(octtoint(str, &actual));
}

TEST_F(octtointTest, IllegalCharacter) {
	const char* str = "5ac2";
	u_long actual;

	ASSERT_FALSE(octtoint(str, &actual));
}

TEST_F(octtointTest, IllegalDigit) {
	const char* str = "5283";
	u_long actual;

	ASSERT_FALSE(octtoint(str, &actual));
}
