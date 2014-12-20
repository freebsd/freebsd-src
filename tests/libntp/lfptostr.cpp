/* 
 * This file contains test for both mfptoa and mfptoms (which uses dolfptoa),
 * since all these functions are very similar. It also tests ulfptoa, which is
 * a macro.
 */

#include "libntptest.h"

extern "C" {
#include "ntp_fp.h"
};

class lfptostrTest : public libntptest {
protected:
	static const int LFP_MAX_PRECISION = 10;
	static const int LFP_MAX_PRECISION_MS = 7;

	static const int ONE_FOURTH = 1073741824; // (1 << 30)
	static const int HALF = (1 << 31);
	static const int THREE_FOURTH = -ONE_FOURTH;
	static const int HALF_PROMILLE_UP = 2147484; // slightly more than 0.0005
	static const int HALF_PROMILLE_DOWN = 2147483; // slightly less than 0.0005
};

TEST_F(lfptostrTest, PositiveInteger) {
	l_fp test = {200, 0}; // exact 200.0000000000

	EXPECT_STREQ("200.0000000000", mfptoa(test.l_ui, test.l_uf, LFP_MAX_PRECISION));
	EXPECT_STREQ("200000.0000000", mfptoms(test.l_ui, test.l_uf, LFP_MAX_PRECISION_MS));
}

TEST_F(lfptostrTest, NegativeInteger) {
	l_fp test = {-100, 0}; // -100

	EXPECT_STREQ("-100.0000000000", lfptoa(&test, LFP_MAX_PRECISION));
	EXPECT_STREQ("-100000.0000000", lfptoms(&test, LFP_MAX_PRECISION_MS));
}

TEST_F(lfptostrTest, PositiveIntegerWithFraction) {
	l_fp test = {200, ONE_FOURTH}; // 200.25

	EXPECT_STREQ("200.2500000000", lfptoa(&test, LFP_MAX_PRECISION));
	EXPECT_STREQ("200250.0000000", lfptoms(&test, LFP_MAX_PRECISION_MS));
}

TEST_F(lfptostrTest, NegativeIntegerWithFraction) {
	l_fp test = {-100, ONE_FOURTH}; // -99.75

	EXPECT_STREQ("-99.7500000000", lfptoa(&test, LFP_MAX_PRECISION));
	EXPECT_STREQ("-99750.0000000", lfptoms(&test, LFP_MAX_PRECISION_MS));
}

TEST_F(lfptostrTest, RoundingDownToInteger) {
	l_fp test = {10, ONE_FOURTH}; // 10.25

	EXPECT_STREQ("10", lfptoa(&test, 0));
	EXPECT_STREQ("10250", lfptoms(&test, 0));
}

TEST_F(lfptostrTest, RoundingMiddleToInteger) {
	l_fp test = {10, HALF}; // 10.5

	EXPECT_STREQ("11", lfptoa(&test, 0));
	EXPECT_STREQ("10500", lfptoms(&test, 0));
}

TEST_F(lfptostrTest, RoundingUpToInteger) {
	l_fp test = {5, THREE_FOURTH}; // 5.75

	EXPECT_STREQ("6", lfptoa(&test, 0));
	EXPECT_STREQ("5750", lfptoms(&test, 0));
}

TEST_F(lfptostrTest, SingleDecimal) {
	l_fp test = {8, ONE_FOURTH}; // 8.25

	EXPECT_STREQ("8.3", lfptoa(&test, 1));
	EXPECT_STREQ("8250.0", lfptoms(&test, 1));
}

TEST_F(lfptostrTest, MillisecondsRoundingUp) {
	l_fp test = {1, HALF_PROMILLE_UP}; //slightly more than 1.0005

	EXPECT_STREQ("1.0", lfptoa(&test, 1));

	EXPECT_STREQ("1000.5", lfptoms(&test, 1));
	EXPECT_STREQ("1001", lfptoms(&test, 0));
}

TEST_F(lfptostrTest, MillisecondsRoundingDown) {
	l_fp test = {1, HALF_PROMILLE_DOWN}; // slightly less than 1.0005

	EXPECT_STREQ("1.0", lfptoa(&test, 1));

	EXPECT_STREQ("1000.5", lfptoms(&test, 1));
	EXPECT_STREQ("1000", lfptoms(&test, 0));
}

TEST_F(lfptostrTest, UnsignedInteger) {
	l_fp test = {3000000000UL, 0};

	EXPECT_STREQ("3000000000.0", ulfptoa(&test, 1));
}
