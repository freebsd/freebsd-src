#include "config.h"

#include "ntp_fp.h"
#include "timevalops.h"

#include "unity.h"

void test_Seconds(void);
void test_MicrosecondsExact(void);
void test_MicrosecondsRounding(void);


void
test_Seconds(void) {
	const l_fp input = {{50}, 0}; /* 50.0 s */
	const struct timeval expected = {50, 0};
	struct timeval actual;

	TSTOTV(&input, &actual);

	TEST_ASSERT_EQUAL(expected.tv_sec, actual.tv_sec);
	TEST_ASSERT_EQUAL(expected.tv_usec, actual.tv_usec);
}

void
test_MicrosecondsExact(void) {
	const u_long HALF = 2147483648UL;
	const l_fp input = {{50}, HALF}; /* 50.5 s */
	const struct timeval expected = {50, 500000};
	struct timeval actual;

	TSTOTV(&input, &actual);

	TEST_ASSERT_EQUAL(expected.tv_sec, actual.tv_sec);
	TEST_ASSERT_EQUAL(expected.tv_usec, actual.tv_usec);

}

void
test_MicrosecondsRounding(void) {
	const l_fp input = {{50}, 3865471UL}; /* Should round to 50.0009 */
	const struct timeval expected = {50, 900};
	struct timeval actual;

	TSTOTV(&input, &actual);

	TEST_ASSERT_EQUAL(expected.tv_sec, actual.tv_sec);
	TEST_ASSERT_EQUAL(expected.tv_usec, actual.tv_usec);
}
