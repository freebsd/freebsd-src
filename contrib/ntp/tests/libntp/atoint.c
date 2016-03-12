#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"
#include "unity.h"

void test_RegularPositive(void);
void test_RegularNegative(void);
void test_PositiveOverflowBoundary(void);
void test_NegativeOverflowBoundary(void);
void test_PositiveOverflowBig(void); 
void test_IllegalCharacter(void);



void test_RegularPositive(void) {
        const char *str = "17";
        long val;

        TEST_ASSERT_TRUE(atoint(str, &val));
        TEST_ASSERT_EQUAL(17, val);
}

void test_RegularNegative(void) {
        const char *str = "-20";
        long val;

        TEST_ASSERT_TRUE(atoint(str, &val));
        TEST_ASSERT_EQUAL(-20, val);
}

void test_PositiveOverflowBoundary(void) {
        const char *str = "2147483648";
        long val;

        TEST_ASSERT_FALSE(atoint(str, &val));
}

void test_NegativeOverflowBoundary(void) {
        const char *str = "-2147483649";
        long val;

        TEST_ASSERT_FALSE(atoint(str, &val));
}

void test_PositiveOverflowBig(void) {
        const char *str = "2300000000";
        long val;

        TEST_ASSERT_FALSE(atoint(str, &val));
}

void test_IllegalCharacter(void) {
        const char *str = "4500l";
        long val;

        TEST_ASSERT_FALSE(atoint(str, &val));
}


