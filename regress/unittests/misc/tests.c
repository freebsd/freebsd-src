/* 	$OpenBSD: tests.c,v 1.13 2025/09/04 00:34:17 djm Exp $ */
/*
 * Regress test for misc helper functions.
 *
 * Placed in the public domain.
 */

#include "includes.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"

#include "log.h"
#include "misc.h"

void test_parse(void);
void test_convtime(void);
void test_expand(void);
void test_argv(void);
void test_strdelim(void);
void test_hpdelim(void);
void test_ptimeout(void);
void test_xextendf(void);
void test_misc(void);

void
tests(void)
{
	test_parse();
	test_convtime();
	test_expand();
	test_argv();
	test_strdelim();
	test_hpdelim();
	test_ptimeout();
	test_xextendf();
	test_misc();
}

void
benchmarks(void)
{
	printf("no benchmarks\n");
}
