/* 	$OpenBSD: tests.c,v 1.4 2021/01/15 02:58:11 dtucker Exp $ */
/*
 * Regress test for misc helper functions.
 *
 * Placed in the public domain.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"

#include "log.h"
#include "misc.h"

void
tests(void)
{
	int port, parseerr;
	char *user, *host, *path, *ret;
	char buf[1024];

	TEST_START("misc_parse_user_host_path");
	ASSERT_INT_EQ(parse_user_host_path("someuser@some.host:some/path",
	    &user, &host, &path), 0);
	ASSERT_STRING_EQ(user, "someuser");
	ASSERT_STRING_EQ(host, "some.host");
	ASSERT_STRING_EQ(path, "some/path");
	free(user); free(host); free(path);
	TEST_DONE();

	TEST_START("misc_parse_user_ipv4_path");
	ASSERT_INT_EQ(parse_user_host_path("someuser@1.22.33.144:some/path",
	    &user, &host, &path), 0);
	ASSERT_STRING_EQ(user, "someuser");
	ASSERT_STRING_EQ(host, "1.22.33.144");
	ASSERT_STRING_EQ(path, "some/path");
	free(user); free(host); free(path);
	TEST_DONE();

	TEST_START("misc_parse_user_[ipv4]_path");
	ASSERT_INT_EQ(parse_user_host_path("someuser@[1.22.33.144]:some/path",
	    &user, &host, &path), 0);
	ASSERT_STRING_EQ(user, "someuser");
	ASSERT_STRING_EQ(host, "1.22.33.144");
	ASSERT_STRING_EQ(path, "some/path");
	free(user); free(host); free(path);
	TEST_DONE();

	TEST_START("misc_parse_user_[ipv4]_nopath");
	ASSERT_INT_EQ(parse_user_host_path("someuser@[1.22.33.144]:",
	    &user, &host, &path), 0);
	ASSERT_STRING_EQ(user, "someuser");
	ASSERT_STRING_EQ(host, "1.22.33.144");
	ASSERT_STRING_EQ(path, ".");
	free(user); free(host); free(path);
	TEST_DONE();

	TEST_START("misc_parse_user_ipv6_path");
	ASSERT_INT_EQ(parse_user_host_path("someuser@[::1]:some/path",
	    &user, &host, &path), 0);
	ASSERT_STRING_EQ(user, "someuser");
	ASSERT_STRING_EQ(host, "::1");
	ASSERT_STRING_EQ(path, "some/path");
	free(user); free(host); free(path);
	TEST_DONE();

	TEST_START("misc_parse_uri");
	ASSERT_INT_EQ(parse_uri("ssh", "ssh://someuser@some.host:22/some/path",
	    &user, &host, &port, &path), 0);
	ASSERT_STRING_EQ(user, "someuser");
	ASSERT_STRING_EQ(host, "some.host");
	ASSERT_INT_EQ(port, 22);
	ASSERT_STRING_EQ(path, "some/path");
	free(user); free(host); free(path);
	TEST_DONE();

	TEST_START("misc_convtime");
	ASSERT_INT_EQ(convtime("0"), 0);
	ASSERT_INT_EQ(convtime("1"), 1);
	ASSERT_INT_EQ(convtime("2s"), 2);
	ASSERT_INT_EQ(convtime("3m"), 180);
	ASSERT_INT_EQ(convtime("1m30"), 90);
	ASSERT_INT_EQ(convtime("1m30s"), 90);
	ASSERT_INT_EQ(convtime("1h1s"), 3601);
	ASSERT_INT_EQ(convtime("1h30m"), 90 * 60);
	ASSERT_INT_EQ(convtime("1d"), 24 * 60 * 60);
	ASSERT_INT_EQ(convtime("1w"), 7 * 24 * 60 * 60);
	ASSERT_INT_EQ(convtime("1w2d3h4m5"), 788645);
	ASSERT_INT_EQ(convtime("1w2d3h4m5s"), 788645);
	/* any negative number or error returns -1 */
	ASSERT_INT_EQ(convtime("-1"),  -1);
	ASSERT_INT_EQ(convtime(""),  -1);
	ASSERT_INT_EQ(convtime("trout"),  -1);
	ASSERT_INT_EQ(convtime("-77"),  -1);
	/* boundary conditions */
	snprintf(buf, sizeof buf, "%llu", (long long unsigned)INT_MAX);
	ASSERT_INT_EQ(convtime(buf), INT_MAX);
	snprintf(buf, sizeof buf, "%llu", (long long unsigned)INT_MAX + 1);
	ASSERT_INT_EQ(convtime(buf), -1);
	ASSERT_INT_EQ(convtime("3550w5d3h14m7s"), 2147483647);
#if INT_MAX == 2147483647
	ASSERT_INT_EQ(convtime("3550w5d3h14m8s"), -1);
#endif
	TEST_DONE();

	TEST_START("dollar_expand");
	if (setenv("FOO", "bar", 1) != 0)
		abort();
	if (setenv("BAR", "baz", 1) != 0)
		abort();
	if (unsetenv("BAZ") != 0)
		abort();
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
