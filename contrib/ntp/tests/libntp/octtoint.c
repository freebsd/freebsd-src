#include "config.h"

#include "ntp_stdlib.h"

#include "unity.h"


void test_SingleDigit(void);
void test_MultipleDigits(void);
void test_Zero(void);
void test_MaximumUnsigned32bit(void);
void test_Overflow(void);
void test_IllegalCharacter(void);
void test_IllegalDigit(void);


void
test_SingleDigit(void)
{
	const char* str = "5";
	u_long actual;

	TEST_ASSERT_TRUE(octtoint(str, &actual));
	TEST_ASSERT_EQUAL(5, actual);

	return;
}

void
test_MultipleDigits(void)
{
	const char* str = "271";
	u_long actual;

	TEST_ASSERT_TRUE(octtoint(str, &actual));
	TEST_ASSERT_EQUAL(185, actual);

	return;
}

void
test_Zero(void)
{
	const char* str = "0";
	u_long actual;

	TEST_ASSERT_TRUE(octtoint(str, &actual));
	TEST_ASSERT_EQUAL(0, actual);

	return;
}

void
test_MaximumUnsigned32bit(void)
{
	const char* str = "37777777777";
	u_long actual;

	TEST_ASSERT_TRUE(octtoint(str, &actual));
	TEST_ASSERT_EQUAL(4294967295UL, actual);

	return;
}

void
test_Overflow(void)
{
	const char* str = "40000000000";
	u_long actual;

	TEST_ASSERT_FALSE(octtoint(str, &actual));

	return;
}

void
test_IllegalCharacter(void)
{
	const char* str = "5ac2";
	u_long actual;

	TEST_ASSERT_FALSE(octtoint(str, &actual));

	return;
}

void
test_IllegalDigit(void)
{
	const char* str = "5283";
	u_long actual;

	TEST_ASSERT_FALSE(octtoint(str, &actual));

	return;
}
