/* 	$OpenBSD: tests.c,v 1.2 2025/04/15 04:00:42 djm Exp $ */
/*
 * Regress test for sshbuf.h buffer API
 *
 * Placed in the public domain
 */

#include "includes.h"

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
