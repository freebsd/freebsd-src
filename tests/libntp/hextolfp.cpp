#include "lfptest.h"

class hextolfpTest : public lfptest {
};

TEST_F(hextolfpTest, PositiveInteger) {
	const char *str = "00001000.00000000";
	l_fp actual;

	l_fp expected = {4096, 0}; // 16^3, no fraction part.

	ASSERT_TRUE(hextolfp(str, &actual));
	EXPECT_TRUE(IsEqual(expected, actual));
}

TEST_F(hextolfpTest, NegativeInteger) {
	const char *str = "ffffffff.00000000"; // -1 decimal
	l_fp actual;

	l_fp expected = {-1, 0};

	ASSERT_TRUE(hextolfp(str, &actual));
	EXPECT_TRUE(IsEqual(expected, actual));
}

TEST_F(hextolfpTest, PositiveFraction) {
	const char *str = "00002000.80000000"; // 8196.5 decimal
	l_fp actual;

	l_fp expected = {8192, HALF};

	ASSERT_TRUE(hextolfp(str, &actual));
	EXPECT_TRUE(IsEqual(expected, actual));
}

TEST_F(hextolfpTest, NegativeFraction) {
	const char *str = "ffffffff.40000000"; // -1 + 0.25 decimal
	l_fp actual;

	l_fp expected = {-1, QUARTER}; //-1 + 0.25

	ASSERT_TRUE(hextolfp(str, &actual));
	EXPECT_TRUE(IsEqual(expected, actual));
}

TEST_F(hextolfpTest, IllegalNumberOfInteger) {
	const char *str = "1000000.00000000"; // Missing one digit in integral part.
	l_fp actual;

	ASSERT_FALSE(hextolfp(str, &actual));
}

TEST_F(hextolfpTest, IllegalChar) {
	const char *str = "10000000.0000h000"; // Illegal character h.
	l_fp actual;

	ASSERT_FALSE(hextolfp(str, &actual));
}
