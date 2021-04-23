/* 	$OpenBSD: tests.c,v 1.6 2021/03/19 04:23:50 djm Exp $ */
/*
 * Regress test for misc helper functions.
 *
 * Placed in the public domain.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/param.h>
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

void
tests(void)
{
	test_parse();
	test_convtime();
	test_expand();
	test_argv();
}
