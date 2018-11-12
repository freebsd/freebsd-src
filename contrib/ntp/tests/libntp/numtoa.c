#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"
#include "ntp_fp.h"

#include "unity.h"


void setUp(void)
{ 
}

void tearDown(void)
{
}

void test_Address(void) {
        u_int32 input = htonl(3221225472UL+512UL+1UL); // 192.0.2.1

        TEST_ASSERT_EQUAL_STRING("192.0.2.1", numtoa(input));
}

void test_Netmask(void) {
        // 255.255.255.0
        u_int32 hostOrder = 255UL*256UL*256UL*256UL + 255UL*256UL*256UL + 255UL*256UL;
        u_int32 input = htonl(hostOrder);

        TEST_ASSERT_EQUAL_STRING("255.255.255.0", numtoa(input));
}

