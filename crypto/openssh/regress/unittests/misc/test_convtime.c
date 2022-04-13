/* 	$OpenBSD: test_convtime.c,v 1.2 2021/12/14 21:25:27 deraadt Exp $ */
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

void test_convtime(void);

void
test_convtime(void)
{
	char buf[1024];

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
}
