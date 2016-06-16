#include "config.h"

#include "ntp_stdlib.h"
#include "isc/string.h"

#include "unity.h"

/* Basisc test for timingsafe_memcmp() */

void test_Empty(void);
void test_Equal(void);
void test_FirstByte(void);
void test_LastByte(void);
void test_MiddleByte(void);
void test_MiddleByteUpLo(void);

void test_Empty(void)
{
	static const char dummy[1];
	TEST_ASSERT_EQUAL_INT(0, isc_tsmemcmp(NULL , NULL , 0));
	TEST_ASSERT_EQUAL_INT(0, isc_tsmemcmp(dummy, dummy, 0));
}

void test_Equal(void)
{
	static const char dummy[2][4] = {
		"blob", "blob"
	};
	TEST_ASSERT_EQUAL_INT(0, isc_tsmemcmp(dummy[0],
					      dummy[1],
					      sizeof(dummy[0])));
}

void test_FirstByte(void)
{
	static const char dummy[2][4] = {
		"Blob", "Clob"
	};
	TEST_ASSERT_EQUAL_INT(-1, isc_tsmemcmp(dummy[0],
					       dummy[1],
					       sizeof(dummy[0])));
	TEST_ASSERT_EQUAL_INT( 1, isc_tsmemcmp(dummy[1],
					       dummy[0],
					       sizeof(dummy[0])));
}

void test_LastByte(void)
{
	static const char dummy[2][4] = {
		"Blob", "Bloc"
	};
	TEST_ASSERT_EQUAL_INT(-1, isc_tsmemcmp(dummy[0],
					       dummy[1],
					       sizeof(dummy[0])));
	TEST_ASSERT_EQUAL_INT( 1, isc_tsmemcmp(dummy[1],
					       dummy[0],
					       sizeof(dummy[0])));
}

void test_MiddleByte(void)
{
	static const char dummy[2][4] = {
		"Blob", "Blpb"
	};
	TEST_ASSERT_EQUAL_INT(-1, isc_tsmemcmp(dummy[0],
					       dummy[1],
					       sizeof(dummy[0])));
	TEST_ASSERT_EQUAL_INT( 1, isc_tsmemcmp(dummy[1],
					       dummy[0],
					       sizeof(dummy[0])));
}

void test_MiddleByteUpLo(void)
{
	static const char dummy[2][4] = {
		"Blob", "Blpa"
	};
	TEST_ASSERT_EQUAL_INT(-1, isc_tsmemcmp(dummy[0],
					       dummy[1],
					       sizeof(dummy[0])));
	TEST_ASSERT_EQUAL_INT( 1, isc_tsmemcmp(dummy[1],
					       dummy[0],
					       sizeof(dummy[0])));
}

