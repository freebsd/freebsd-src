#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"
#include "ntp_fp.h"

#include "unity.h"

void setUp(void);
void test_ConstantDate(void);


void
setUp(void)
{
	init_lib();

	return;
}


void
test_ConstantDate(void) {
	const u_int32 HALF = 2147483648UL;

	l_fp e_time = {{3485080800UL}, HALF}; /* 2010-06-09 14:00:00.5 */

	TEST_ASSERT_EQUAL_STRING("cfba1ce0.80000000  Wed, Jun  9 2010 14:00:00.500",
		gmprettydate(&e_time));
	return;
}
