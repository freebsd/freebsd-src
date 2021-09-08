/* 	$OpenBSD: test_expand.c,v 1.2 2021/04/06 09:07:33 dtucker Exp $ */
/*
 * Regress test for misc string expansion functions.
 *
 * Placed in the public domain.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"

#include "log.h"
#include "misc.h"

void test_expand(void);

void
test_expand(void)
{
	int parseerr;
	char *ret;

	TEST_START("dollar_expand");
	ASSERT_INT_EQ(setenv("FOO", "bar", 1), 0);
	ASSERT_INT_EQ(setenv("BAR", "baz", 1), 0);
	(void)unsetenv("BAZ");
#define ASSERT_DOLLAR_EQ(x, y) do { \
	char *str = dollar_expand(NULL, (x)); \
	ASSERT_STRING_EQ(str, (y)); \
	free(str); \
} while(0)
	ASSERT_DOLLAR_EQ("${FOO}", "bar");
	ASSERT_DOLLAR_EQ(" ${FOO}", " bar");
	ASSERT_DOLLAR_EQ("${FOO} ", "bar ");
	ASSERT_DOLLAR_EQ(" ${FOO} ", " bar ");
	ASSERT_DOLLAR_EQ("${FOO}${BAR}", "barbaz");
	ASSERT_DOLLAR_EQ(" ${FOO} ${BAR}", " bar baz");
	ASSERT_DOLLAR_EQ("${FOO}${BAR} ", "barbaz ");
	ASSERT_DOLLAR_EQ(" ${FOO} ${BAR} ", " bar baz ");
	ASSERT_DOLLAR_EQ("$", "$");
	ASSERT_DOLLAR_EQ(" $", " $");
	ASSERT_DOLLAR_EQ("$ ", "$ ");

	/* suppress error messages for error handing tests */
	log_init("test_misc", SYSLOG_LEVEL_QUIET, SYSLOG_FACILITY_AUTH, 1);
	/* error checking, non existent variable */
	ret = dollar_expand(&parseerr, "a${BAZ}");
	ASSERT_PTR_EQ(ret, NULL); ASSERT_INT_EQ(parseerr, 0);
	ret = dollar_expand(&parseerr, "${BAZ}b");
	ASSERT_PTR_EQ(ret, NULL); ASSERT_INT_EQ(parseerr, 0);
	ret = dollar_expand(&parseerr, "a${BAZ}b");
	ASSERT_PTR_EQ(ret, NULL); ASSERT_INT_EQ(parseerr, 0);
	/* invalid format */
	ret = dollar_expand(&parseerr, "${");
	ASSERT_PTR_EQ(ret, NULL); ASSERT_INT_EQ(parseerr, 1);
	ret = dollar_expand(&parseerr, "${F");
	ASSERT_PTR_EQ(ret, NULL); ASSERT_INT_EQ(parseerr, 1);
	ret = dollar_expand(&parseerr, "${FO");
	ASSERT_PTR_EQ(ret, NULL); ASSERT_INT_EQ(parseerr, 1);
	/* empty variable name */
	ret = dollar_expand(&parseerr, "${}");
	ASSERT_PTR_EQ(ret, NULL); ASSERT_INT_EQ(parseerr, 1);
	/* restore loglevel to default */
	log_init("test_misc", SYSLOG_LEVEL_INFO, SYSLOG_FACILITY_AUTH, 1);
	TEST_DONE();

	TEST_START("percent_expand");
	ASSERT_STRING_EQ(percent_expand("%%", "%h", "foo", NULL), "%");
	ASSERT_STRING_EQ(percent_expand("%h", "h", "foo", NULL), "foo");
	ASSERT_STRING_EQ(percent_expand("%h ", "h", "foo", NULL), "foo ");
	ASSERT_STRING_EQ(percent_expand(" %h", "h", "foo", NULL), " foo");
	ASSERT_STRING_EQ(percent_expand(" %h ", "h", "foo", NULL), " foo ");
	ASSERT_STRING_EQ(percent_expand(" %a%b ", "a", "foo", "b", "bar", NULL),
	    " foobar ");
	TEST_DONE();

	TEST_START("percent_dollar_expand");
	ASSERT_STRING_EQ(percent_dollar_expand("%h${FOO}", "h", "foo", NULL),
	    "foobar");
	TEST_DONE();
}
