#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_fp.h"

#include "unity.h"

void test_Address(void);
void test_Netmask(void);

void
test_Address(void) {
	const u_int32 input = htonl(3221225472UL + 512UL + 1UL); // 192.0.2.1

	TEST_ASSERT_EQUAL_STRING("192.0.2.1", numtoa(input));
}

void
test_Netmask(void) {
	// 255.255.255.0
	const u_int32 hostOrder = 255UL*256UL*256UL*256UL + 255UL*256UL*256UL + 255UL*256UL;
	const u_int32 input = htonl(hostOrder);

	TEST_ASSERT_EQUAL_STRING("255.255.255.0", numtoa(input));
}
