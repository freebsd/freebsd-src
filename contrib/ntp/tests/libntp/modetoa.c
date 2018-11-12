#include "config.h"

#include "ntp_stdlib.h"

#include "unity.h"

void setUp(void);
void test_KnownMode(void);
void test_UnknownMode(void);


void
setUp(void)
{
	init_lib();

	return;
}


void
test_KnownMode(void) {
	const int MODE = 3; // Should be "client"

	TEST_ASSERT_EQUAL_STRING("client", modetoa(MODE));
}

void
test_UnknownMode(void) {
	const int MODE = 100;

	TEST_ASSERT_EQUAL_STRING("mode#100", modetoa(MODE));
}
