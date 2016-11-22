/* 
 * This file contains test for both fptoa and fptoms (which uses dofptoa),
 * since all these functions are very similar.
 */
#include "config.h"
#include "ntp_fp.h"
#include "ntp_stdlib.h"
#include "unity.h"
 
#define SFP_MAX_PRECISION 6

void setUp(void);
void test_PositiveInteger(void);
void test_NegativeInteger(void);
void test_PositiveIntegerPositiveFraction(void);
void test_NegativeIntegerNegativeFraction(void);
void test_PositiveIntegerNegativeFraction(void);
void test_NegativeIntegerPositiveFraction(void);
void test_SingleDecimalInteger(void);
void test_SingleDecimalRounding(void);


void
setUp(void)
{
	init_lib();

	return;
}


void test_PositiveInteger(void)
{
	s_fp test = 300 << 16; // exact 300.000000

	TEST_ASSERT_EQUAL_STRING("300.000000", fptoa(test, SFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("300000.000", fptoms(test, SFP_MAX_PRECISION));
}

void test_NegativeInteger(void)
{
	s_fp test = -(200 << 16); // exact -200.000000

	TEST_ASSERT_EQUAL_STRING("-200.000000", fptoa(test, SFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("-200000.000", fptoms(test, SFP_MAX_PRECISION));
}

void test_PositiveIntegerPositiveFraction(void)
{
	s_fp test = (300 << 16) + (1 << 15); // 300 + 0.5

	TEST_ASSERT_EQUAL_STRING("300.500000", fptoa(test, SFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("300500.000", fptoms(test, SFP_MAX_PRECISION));
}

void test_NegativeIntegerNegativeFraction(void)
{
	s_fp test = -(200 << 16) - (1 << 15); // -200 - 0.5

	TEST_ASSERT_EQUAL_STRING("-200.500000", fptoa(test, SFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("-200500.000", fptoms(test, SFP_MAX_PRECISION));
}

void test_PositiveIntegerNegativeFraction(void)
{
	s_fp test = (300 << 16) - (1 << 14); // 300 - 0.25

	TEST_ASSERT_EQUAL_STRING("299.750000", fptoa(test, SFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("299750.000", fptoms(test, SFP_MAX_PRECISION));
}

void test_NegativeIntegerPositiveFraction(void)
{
	s_fp test = -(200 << 16) + (1 << 14)*3; // -200 + 0.75

	TEST_ASSERT_EQUAL_STRING("-199.250000", fptoa(test, SFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("-199250.000", fptoms(test, SFP_MAX_PRECISION));
}

void test_SingleDecimalInteger(void)
{
	s_fp test = 300 << 16; // 300

	TEST_ASSERT_EQUAL_STRING("300.0", fptoa(test, 1));
	TEST_ASSERT_EQUAL_STRING("300000.0", fptoms(test, 1));
}

void test_SingleDecimalRounding(void)
{
	s_fp test = (2 << 16) + (1 << 14)*3; // 2 + 0.25*3 = 2.75

	TEST_ASSERT_EQUAL_STRING("2.8", fptoa(test, 1));
	TEST_ASSERT_EQUAL_STRING("2750.0", fptoms(test, 1));
}
