/* 	$OpenBSD: tests.c,v 1.2 2025/04/15 04:00:42 djm Exp $ */
/*
 * Regress test for sshbuf.h buffer API
 *
 * Placed in the public domain
 */

#include "includes.h"

#include <stdio.h>

#include "../test_helper/test_helper.h"

void sshbuf_tests(void);
void sshbuf_getput_basic_tests(void);
void sshbuf_getput_crypto_tests(void);
void sshbuf_misc_tests(void);
void sshbuf_fuzz_tests(void);
void sshbuf_getput_fuzz_tests(void);
void sshbuf_fixed(void);

void
tests(void)
{
	sshbuf_tests();
	sshbuf_getput_basic_tests();
#ifdef WITH_OPENSSL
	sshbuf_getput_crypto_tests();
#endif
	sshbuf_misc_tests();
	sshbuf_fuzz_tests();
	sshbuf_getput_fuzz_tests();
	sshbuf_fixed();
}

void
benchmarks(void)
{
	printf("no benchmarks\n");
}
