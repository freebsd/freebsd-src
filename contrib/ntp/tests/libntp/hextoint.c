#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"
#include "ntp_fp.h"

#include "unity.h"


void test_SingleDigit(void) {
        const char *str = "a"; // 10 decimal
        u_long actual;

        TEST_ASSERT_TRUE(hextoint(str, &actual));
        TEST_ASSERT_EQUAL(10, actual);
}

void test_MultipleDigits(void) {
        const char *str = "8F3"; // 2291 decimal
        u_long actual;

        TEST_ASSERT_TRUE(hextoint(str, &actual));
        TEST_ASSERT_EQUAL(2291, actual);
}

void test_MaxUnsigned(void) {
        const char *str = "ffffffff"; // 4294967295 decimal
        u_long actual;

        TEST_ASSERT_TRUE(hextoint(str, &actual));
        TEST_ASSERT_EQUAL(4294967295UL, actual);
}

void test_Overflow(void) {
        const char *str = "100000000"; // Overflow by 1
        u_long actual;

        TEST_ASSERT_FALSE(hextoint(str, &actual));
}

void test_IllegalChar(void) {
        const char *str = "5gb"; // Illegal character g
        u_long actual;

        TEST_ASSERT_FALSE(hextoint(str, &actual));
}

