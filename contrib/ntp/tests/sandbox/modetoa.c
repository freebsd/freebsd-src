//#include "config.h"
//#include "libntptest.h"
#include "unity.h"
//#include "ntp_stdlib.h"



void test_KnownMode(void) {
	const int MODE = 3; // Should be "client"
	TEST_ASSERT_EQUAL_STRING("client", modetoa(MODE));

//	EXPECT_STREQ("client", modetoa(MODE));
}

void test_UnknownMode(void) {
	const int MODE = 100;

	 TEST_ASSERT_EQUAL_STRING("mode#1001", modetoa(MODE));
//	EXPECT_STREQ("mode#100", modetoa(MODE));
}
