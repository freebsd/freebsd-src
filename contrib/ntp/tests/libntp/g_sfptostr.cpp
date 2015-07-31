/* 
 * This file contains test for both fptoa and fptoms (which uses dofptoa),
 * since all these functions are very similar.
 */

#include "g_libntptest.h"

extern "C" {
#include "ntp_fp.h"
};

class sfptostr : public libntptest {
protected:
	static const int SFP_MAX_PRECISION = 6;
};

TEST_F(sfptostr, PositiveInteger) {
	s_fp test = 300 << 16; // exact 300.000000

	EXPECT_STREQ("300.000000", fptoa(test, SFP_MAX_PRECISION));
	EXPECT_STREQ("300000.000", fptoms(test, SFP_MAX_PRECISION));
}

TEST_F(sfptostr, NegativeInteger) {
	s_fp test = -200 << 16; // exact -200.000000

	EXPECT_STREQ("-200.000000", fptoa(test, SFP_MAX_PRECISION));
	EXPECT_STREQ("-200000.000", fptoms(test, SFP_MAX_PRECISION));
}

TEST_F(sfptostr, PositiveIntegerPositiveFraction) {
	s_fp test = (300 << 16) + (1 << 15); // 300 + 0.5

	EXPECT_STREQ("300.500000", fptoa(test, SFP_MAX_PRECISION));
	EXPECT_STREQ("300500.000", fptoms(test, SFP_MAX_PRECISION));
}

TEST_F(sfptostr, NegativeIntegerNegativeFraction) {
	s_fp test = (-200 << 16) - (1 << 15); // -200 - 0.5

	EXPECT_STREQ("-200.500000", fptoa(test, SFP_MAX_PRECISION));
	EXPECT_STREQ("-200500.000", fptoms(test, SFP_MAX_PRECISION));
}

TEST_F(sfptostr, PositiveIntegerNegativeFraction) {
	s_fp test = (300 << 16) - (1 << 14); // 300 - 0.25

	EXPECT_STREQ("299.750000", fptoa(test, SFP_MAX_PRECISION));
	EXPECT_STREQ("299750.000", fptoms(test, SFP_MAX_PRECISION));
}

TEST_F(sfptostr, NegativeIntegerPositiveFraction) {
	s_fp test = (-200 << 16) + (1 << 14)*3; // -200 + 0.75

	EXPECT_STREQ("-199.250000", fptoa(test, SFP_MAX_PRECISION));
	EXPECT_STREQ("-199250.000", fptoms(test, SFP_MAX_PRECISION));
}

TEST_F(sfptostr, SingleDecimalInteger) {
	s_fp test = 300 << 16; // 300

	EXPECT_STREQ("300.0", fptoa(test, 1));
	EXPECT_STREQ("300000.0", fptoms(test, 1));
}

TEST_F(sfptostr, SingleDecimalRounding) {
	s_fp test = (2 << 16) + (1 << 14)*3; // 2 + 0.25*3 = 2.75

	EXPECT_STREQ("2.8", fptoa(test, 1));
	EXPECT_STREQ("2750.0", fptoms(test, 1));
}
