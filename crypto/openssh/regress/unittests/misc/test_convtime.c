/* 	$OpenBSD: test_convtime.c,v 1.3 2022/08/11 01:57:50 djm Exp $ */
/*
 * Regress test for misc time conversion functions.
 *
 * Placed in the public domain.
 */

#include "includes.h"

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"

#include "log.h"
#include "misc.h"
#include "ssherr.h"

void test_convtime(void);

void
test_convtime(void)
{
	char buf[1024];
	uint64_t t;

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

	/* XXX timezones/DST make verification of this tricky */
	/* XXX maybe setenv TZ and tzset() to make it unambiguous? */
	TEST_START("misc_parse_absolute_time");
	ASSERT_INT_EQ(parse_absolute_time("20000101", &t), 0);
	ASSERT_INT_EQ(parse_absolute_time("200001011223", &t), 0);
	ASSERT_INT_EQ(parse_absolute_time("20000101122345", &t), 0);

	/* forced UTC TZ */
	ASSERT_INT_EQ(parse_absolute_time("20000101Z", &t), 0);
	ASSERT_U64_EQ(t, 946684800);
	ASSERT_INT_EQ(parse_absolute_time("200001011223Z", &t), 0);
	ASSERT_U64_EQ(t, 946729380);
	ASSERT_INT_EQ(parse_absolute_time("20000101122345Z", &t), 0);
	ASSERT_U64_EQ(t, 946729425);
	ASSERT_INT_EQ(parse_absolute_time("20000101UTC", &t), 0);
	ASSERT_U64_EQ(t, 946684800);
	ASSERT_INT_EQ(parse_absolute_time("200001011223UTC", &t), 0);
	ASSERT_U64_EQ(t, 946729380);
	ASSERT_INT_EQ(parse_absolute_time("20000101122345UTC", &t), 0);
	ASSERT_U64_EQ(t, 946729425);

	/* Bad month */
	ASSERT_INT_EQ(parse_absolute_time("20001301", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("20000001", &t),
	    SSH_ERR_INVALID_FORMAT);
	/* Incomplete */
	ASSERT_INT_EQ(parse_absolute_time("2", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("2000", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("20000", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("200001", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("2000010", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("200001010", &t),
	    SSH_ERR_INVALID_FORMAT);
	/* Bad day, hour, minute, second */
	ASSERT_INT_EQ(parse_absolute_time("20000199", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("200001019900", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("200001010099", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("20000101000099", &t),
	    SSH_ERR_INVALID_FORMAT);
	/* Invalid TZ specifier */
	ASSERT_INT_EQ(parse_absolute_time("20000101ZZ", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("20000101PDT", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("20000101U", &t),
	    SSH_ERR_INVALID_FORMAT);
	ASSERT_INT_EQ(parse_absolute_time("20000101UTCUTC", &t),
	    SSH_ERR_INVALID_FORMAT);

	TEST_DONE();
}
