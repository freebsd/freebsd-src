#include "config.h"

#include "ntp_stdlib.h"
#include "vint64ops.h"

#include "unity.h"


int IsEqual(const vint64 expected, const vint64 actual);
void test_ParseVUI64_pos(void);
void test_ParseVUI64_neg(void);
void test_ParseVUI64_case(void);


// technically bool
int
IsEqual(const vint64 expected, const vint64 actual) {
	if (0 == memcmp(&expected, &actual, sizeof(vint64))) {
		printf( "%x.", expected.D_s.hi);
		printf("%x", expected.D_s.lo);
		printf(" but was ");
		printf("%x.", actual.D_s.hi);
		printf("%x\n", actual.D_s.lo);
		return TRUE;
	} else {
		printf("expected: ");
		printf( "%d.", expected.D_s.hi);
		printf("%d", expected.D_s.lo);
		printf(" but was ");
		printf("%d", actual.D_s.lo);
		printf("%d", actual.D_s.lo);
		return FALSE;
	}
}

// ----------------------------------------------------------------------
// test number parser
void
test_ParseVUI64_pos(void) {
	vint64 act, exp;
	const char *sp;
	char       *ep;

	sp         = "1234x";
	exp.D_s.hi = 0;
	exp.D_s.lo = 1234;
	act        = strtouv64(sp, &ep, 0);

	TEST_ASSERT_TRUE(IsEqual(exp, act));
	TEST_ASSERT_EQUAL(*ep, 'x');
}


void
test_ParseVUI64_neg(void) {
	vint64 act, exp;
	const char *sp;
	char       *ep;

	sp         = "-1234x";
	exp.D_s.hi = ~0;
	exp.D_s.lo = -1234;
	act        = strtouv64(sp, &ep, 0);
	TEST_ASSERT_TRUE(IsEqual(exp, act));
	TEST_ASSERT_EQUAL(*ep, 'x');
}

void
test_ParseVUI64_case(void) {
	vint64 act, exp;
	const char *sp;
	char       *ep;

	sp         = "0123456789AbCdEf";
	exp.D_s.hi = 0x01234567;
	exp.D_s.lo = 0x89ABCDEF;
	act        = strtouv64(sp, &ep, 16);
	TEST_ASSERT_TRUE(IsEqual(exp, act));
	TEST_ASSERT_EQUAL(*ep, '\0');
}
