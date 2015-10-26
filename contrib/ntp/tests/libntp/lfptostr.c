/* 
 * This file contains test for both mfptoa and mfptoms (which uses dolfptoa),
 * since all these functions are very similar. It also tests ulfptoa, which is
 * a macro.
 */

#include "config.h"
#include "ntp_stdlib.h"
#include "ntp_fp.h"

#include "unity.h"

static const int LFP_MAX_PRECISION = 10;
static const int LFP_MAX_PRECISION_MS = 7;

static const int ONE_FOURTH = 1073741824; /* (1 << 30) */
static const int HALF = (1 << 31);
static const int THREE_FOURTH = -1073741824;
static const int HALF_PROMILLE_UP = 2147484; /* slightly more than 0.0005 */
static const int HALF_PROMILLE_DOWN = 2147483; /* slightly less than 0.0005 */


void test_PositiveInteger(void);
void test_NegativeInteger(void);
void test_PositiveIntegerWithFraction(void);
void test_NegativeIntegerWithFraction(void);
void test_RoundingDownToInteger(void);
void test_RoundingMiddleToInteger(void);
void test_RoundingUpToInteger(void);
void test_SingleDecimal(void);
void test_MillisecondsRoundingUp(void);
void test_MillisecondsRoundingDown(void);
void test_UnsignedInteger(void);



void
test_PositiveInteger(void) {
	l_fp test = {{200}, 0}; /* exact 200.0000000000 */

	TEST_ASSERT_EQUAL_STRING("200.0000000000", mfptoa(test.l_ui, test.l_uf, LFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("200000.0000000", mfptoms(test.l_ui, test.l_uf, LFP_MAX_PRECISION_MS));
}

void
test_NegativeInteger(void) {
	l_fp test = {{-100}, 0}; /* -100 */

	TEST_ASSERT_EQUAL_STRING("-100.0000000000", lfptoa(&test, LFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("-100000.0000000", lfptoms(&test, LFP_MAX_PRECISION_MS));
}

void
test_PositiveIntegerWithFraction(void) {
	l_fp test = {{200}, ONE_FOURTH}; /* 200.25 */

	TEST_ASSERT_EQUAL_STRING("200.2500000000", lfptoa(&test, LFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("200250.0000000", lfptoms(&test, LFP_MAX_PRECISION_MS));
}

void
test_NegativeIntegerWithFraction(void) {
	l_fp test = {{-100}, ONE_FOURTH}; /* -99.75 */

	TEST_ASSERT_EQUAL_STRING("-99.7500000000", lfptoa(&test, LFP_MAX_PRECISION));
	TEST_ASSERT_EQUAL_STRING("-99750.0000000", lfptoms(&test, LFP_MAX_PRECISION_MS));
}

void
test_RoundingDownToInteger(void) {
	l_fp test = {{10}, ONE_FOURTH}; /* 10.25 */

	TEST_ASSERT_EQUAL_STRING("10", lfptoa(&test, 0));
	TEST_ASSERT_EQUAL_STRING("10250", lfptoms(&test, 0));
}

void
test_RoundingMiddleToInteger(void) {
	l_fp test = {{10}, HALF}; /* 10.5 */

	TEST_ASSERT_EQUAL_STRING("11", lfptoa(&test, 0));
	TEST_ASSERT_EQUAL_STRING("10500", lfptoms(&test, 0));
}

void
test_RoundingUpToInteger(void) {
	l_fp test = {{5}, THREE_FOURTH}; /* 5.75 */

	TEST_ASSERT_EQUAL_STRING("6", lfptoa(&test, 0));
	TEST_ASSERT_EQUAL_STRING("5750", lfptoms(&test, 0));
}

void
test_SingleDecimal(void) {
	l_fp test = {{8}, ONE_FOURTH}; /* 8.25 */

	TEST_ASSERT_EQUAL_STRING("8.3", lfptoa(&test, 1));
	TEST_ASSERT_EQUAL_STRING("8250.0", lfptoms(&test, 1));
}

void
test_MillisecondsRoundingUp(void) {
	l_fp test = {{1}, HALF_PROMILLE_UP}; /* slightly more than 1.0005 */

	TEST_ASSERT_EQUAL_STRING("1.0", lfptoa(&test, 1));

	TEST_ASSERT_EQUAL_STRING("1000.5", lfptoms(&test, 1));
	TEST_ASSERT_EQUAL_STRING("1001", lfptoms(&test, 0));
}

void
test_MillisecondsRoundingDown(void) {
	l_fp test = {{1}, HALF_PROMILLE_DOWN}; /* slightly less than 1.0005 */

	TEST_ASSERT_EQUAL_STRING("1.0", lfptoa(&test, 1));

	TEST_ASSERT_EQUAL_STRING("1000.5", lfptoms(&test, 1));
	TEST_ASSERT_EQUAL_STRING("1000", lfptoms(&test, 0));
}

void test_UnsignedInteger(void) {
	l_fp test = {{3000000000UL}, 0};

	TEST_ASSERT_EQUAL_STRING("3000000000.0", ulfptoa(&test, 1));
}
