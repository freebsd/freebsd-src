#include "libntptest.h"

extern "C" {
#include "ntp_fp.h"
#include "timevalops.h"
};

class tstotvTest : public libntptest {
protected:
	::testing::AssertionResult IsEqual(const timeval& expected,
									   const timeval& actual) {
		if (expected.tv_sec == actual.tv_sec &&
			expected.tv_usec == actual.tv_usec) {
			// Success
			return ::testing::AssertionSuccess();
		} else {
			return ::testing::AssertionFailure()
				<< "expected: " << expected.tv_sec << "."
				<< expected.tv_usec
				<< " but was: " << actual.tv_sec << "."
				<< actual.tv_usec;
		}
	}

	static const u_long HALF = 2147483648UL;
};

TEST_F(tstotvTest, Seconds) {
	const l_fp input = {50, 0}; // 50.0 s
	const timeval expected = {50, 0};
	timeval actual;

	TSTOTV(&input, &actual);

	EXPECT_TRUE(IsEqual(expected, actual));
}

TEST_F(tstotvTest, MicrosecondsExact) {
	const l_fp input = {50, HALF}; // 10.5 s
	const timeval expected = {50, 500000};
	timeval actual;

	TSTOTV(&input, &actual);

	EXPECT_TRUE(IsEqual(expected, actual));

}

TEST_F(tstotvTest, MicrosecondsRounding) {
	const l_fp input = {50, 3865471UL}; // Should round to 50.0009
	const timeval expected = {50, 900};
	timeval actual;

	TSTOTV(&input, &actual);

	EXPECT_TRUE(IsEqual(expected, actual));
}
