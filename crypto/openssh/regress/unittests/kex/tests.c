/* 	$OpenBSD: tests.c,v 1.4 2025/04/15 04:00:42 djm Exp $ */
/*
 * Placed in the public domain
 */

#include "../test_helper/test_helper.h"

void kex_tests(void);
void kex_proposal_tests(void);
void kex_proposal_populate_tests(void);

void
tests(void)
{
	kex_tests();
	kex_proposal_tests();
	kex_proposal_populate_tests();
}

void
benchmarks(void)
{
	printf("\n");
	kex_tests();
}
