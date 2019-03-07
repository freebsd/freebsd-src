#include "config.h"
#include "ntp_stdlib.h"
#include "unity.h"

#include <errno.h>

extern void test_NullBuf1(void);
void test_NullBuf1(void)
{
	int rc = xsbprintf(NULL, NULL, "blah");
        TEST_ASSERT(rc == -1 && errno == EINVAL);
}

extern void test_NullBuf2(void);
void test_NullBuf2(void)
{
	char *bp = NULL;
	int   rc = xsbprintf(&bp, NULL, "blah");
        TEST_ASSERT(rc == -1 && errno == EINVAL);
	TEST_ASSERT_EQUAL_PTR(bp, NULL);
}

extern void test_EndBeyond(void);
void test_EndBeyond(void)
{
	char ba[2];
	char *bp = ba + 1;
	char *ep = ba;
	int rc = xsbprintf(&bp, ep, "blah");
        TEST_ASSERT(rc == -1 && errno == EINVAL);
}

extern void test_SmallBuf(void);
void test_SmallBuf(void)
{
	char  ba[4];
	char *bp = ba;
	char *ep = ba + sizeof(ba);
	int   rc = xsbprintf(&bp, ep, "1234");
        TEST_ASSERT(rc == 0 && strlen(ba) == 0);
	TEST_ASSERT_EQUAL_PTR(bp, ba);
}

extern void test_MatchBuf(void);
void test_MatchBuf(void)
{
	char  ba[5];
	char *bp = ba;
	char *ep = ba + sizeof(ba);
	int   rc = xsbprintf(&bp, ep, "1234");
        TEST_ASSERT(rc == 4 && strlen(ba) == 4);
	TEST_ASSERT_EQUAL_PTR(bp, ba + 4);
}

extern void test_BigBuf(void);
void test_BigBuf(void)
{
	char  ba[10];
	char *bp = ba;
	char *ep = ba + sizeof(ba);
	int   rc = xsbprintf(&bp, ep, "1234");
        TEST_ASSERT(rc == 4 && strlen(ba) == 4);
	TEST_ASSERT_EQUAL_PTR(bp, ba + 4);
}

extern void test_SimpleArgs(void);
void test_SimpleArgs(void)
{
	char  ba[10];
	char *bp = ba;
	char *ep = ba + sizeof(ba);
	int   rc = xsbprintf(&bp, ep, "%d%d%d%d", 1, 2, 3, 4);
        TEST_ASSERT(rc == 4 && strlen(ba) == 4);
	TEST_ASSERT_EQUAL_PTR(bp, ba + 4);
	TEST_ASSERT_FALSE(strcmp(ba, "1234"));
}

extern void test_Increment1(void);
void test_Increment1(void)
{
	char  ba[10];
	char *bp = ba;
	char *ep = ba + sizeof(ba);
	int   rc;

	rc = xsbprintf(&bp, ep, "%d%d%d%d", 1, 2, 3, 4);
        TEST_ASSERT(rc == 4 && strlen(ba) == 4);
	TEST_ASSERT_EQUAL_PTR(bp, ba + 4);

	rc = xsbprintf(&bp, ep, "%s", "frob");
        TEST_ASSERT(rc == 4 && strlen(ba) == 8);
	TEST_ASSERT_EQUAL_PTR(bp, ba + 8);
	TEST_ASSERT_FALSE(strcmp(ba, "1234frob"));
}

