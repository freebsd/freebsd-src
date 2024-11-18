/*-
 * Copyright (c) 2011 David Schultz
 * Copyright (c) 2024 Robert Clausecker <fuz@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * BUFSIZE is the number of bytes of rc4 output to compare.  The probability
 * that this test fails spuriously is 2**(-BUFSIZE * 8).
 */
#define	BUFSIZE		8

/*
 * Test whether arc4random_buf() returns the same sequence of bytes in both
 * parent and child processes.  (Hint: It shouldn't.)
 */
ATF_TC_WITHOUT_HEAD(test_arc4random);
ATF_TC_BODY(test_arc4random, tc)
{
	struct shared_page {
		char parentbuf[BUFSIZE];
		char childbuf[BUFSIZE];
	} *page;
	pid_t pid;
	char c;

	page = mmap(NULL, sizeof(struct shared_page), PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_SHARED, -1, 0);
	ATF_REQUIRE_MSG(page != MAP_FAILED, "mmap failed; errno=%d", errno);

	arc4random_buf(&c, 1);

	pid = fork();
	ATF_REQUIRE(0 <= pid);
	if (pid == 0) {
		/* child */
		arc4random_buf(page->childbuf, BUFSIZE);
		exit(0);
	} else {
		/* parent */
		int status;
		arc4random_buf(page->parentbuf, BUFSIZE);
		wait(&status);
	}
	ATF_CHECK_MSG(memcmp(page->parentbuf, page->childbuf, BUFSIZE) != 0,
	    "sequences are the same");
}

/*
 * Test whether arc4random_uniform() returns a number below the given threshold.
 * Test with various thresholds.
 */
ATF_TC_WITHOUT_HEAD(test_arc4random_uniform);
ATF_TC_BODY(test_arc4random_uniform, tc)
{
	size_t i, j;
	static const uint32_t thresholds[] = {
		1, 2, 3, 4, 5, 10, 100, 1000,
		INT32_MAX, (uint32_t)INT32_MAX + 1,
		UINT32_MAX - 1000000000, UINT32_MAX - 1000000, UINT32_MAX - 1, 0
	};

	for (i = 0; thresholds[i] != 0; i++)
		for (j = 0; j < 10000; j++)
			ATF_CHECK(arc4random_uniform(thresholds[i]) < thresholds[i]);

	/* for a threshold of zero, just check that we get zero every time */
	for (j = 0; j < 1000; j++)
		ATF_CHECK_EQ(0, arc4random_uniform(0));
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, test_arc4random);
	ATF_TP_ADD_TC(tp, test_arc4random_uniform);

	return (atf_no_error());
}
