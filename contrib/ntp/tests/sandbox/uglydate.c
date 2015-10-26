#include "config.h"
#include "ntp_fp.h"
#include "unity.h"

//#include "ntp_stdlib.h"
//#include "libntptest.h"

void
test_ConstantDateTime(void) {
	const u_int32 HALF = 2147483648UL;

	l_fp time = {3485080800UL, HALF}; // 2010-06-09 14:00:00.5

	TEST_ASSERT_EQUAL_STRING("3485080800.500000 10:159:14:00:00.500",
				 uglydate(&time));
}
