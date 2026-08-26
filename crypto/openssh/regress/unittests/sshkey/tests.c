/* 	$OpenBSD: tests.c,v 1.3 2026/06/29 07:46:22 djm Exp $ */
/*
 * Regress test for sshbuf.h buffer API
 *
 * Placed in the public domain
 */

#include "includes.h"

#include <stdio.h>

#include "../test_helper/test_helper.h"

void sshkey_tests(void);
void sshkey_file_tests(void);
void sshkey_fuzz_tests(void);
void sshkey_benchmarks(void);

void
tests(void)
{
	sshkey_tests();
	sshkey_file_tests();
	sshkey_fuzz_tests();
}

void
benchmarks(void)
{
	printf("\n");
	sshkey_benchmarks();
}
