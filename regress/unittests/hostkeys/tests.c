/* 	$OpenBSD: tests.c,v 1.2 2025/04/15 04:00:42 djm Exp $ */
/*
 * Regress test for known_hosts-related API.
 *
 * Placed in the public domain
 */

#include "includes.h"

#include <stdio.h>

#include "../test_helper/test_helper.h"

void tests(void);
void test_iterate(void); /* test_iterate.c */

void
tests(void)
{
	test_iterate();
}

void
benchmarks(void)
{
	printf("no benchmarks\n");
}
