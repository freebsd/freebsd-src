/* $NetBSD: t_mlock.c,v 1.5 2014/02/26 20:49:26 martin Exp $ */

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_mlock.c,v 1.5 2014/02/26 20:49:26 martin Exp $");

#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <errno.h>
#include <atf-c.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static long page = 0;

ATF_TC(mlock_clip);
ATF_TC_HEAD(mlock_clip, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test with mlock(2) that UVM only "
	    "clips if the clip address is within the entry (PR kern/44788)");
}

ATF_TC_BODY(mlock_clip, tc)
{
	void *buf;

	buf = malloc(page);
	ATF_REQUIRE(buf != NULL);

	if (page < 1024)
		atf_tc_skip("page size too small");

	for (size_t i = page; i >= 1; i = i - 1024) {
		(void)mlock(buf, page - i);
		(void)munlock(buf, page - i);
	}

	free(buf);
}

ATF_TC(mlock_err);
ATF_TC_HEAD(mlock_err, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test error conditions in mlock(2) and munlock(2)");
}

ATF_TC_BODY(mlock_err, tc)
{
	unsigned long vmin = 0;
	size_t len = sizeof(vmin);
	void *invalid_ptr;
	int null_errno = ENOMEM;	/* error expected for NULL */

	if (sysctlbyname("vm.minaddress", &vmin, &len, NULL, 0) != 0)
		atf_tc_fail("failed to read vm.minaddress");

	if (vmin > 0)
		null_errno = EINVAL;	/* NULL is not inside user VM */

	errno = 0;
	ATF_REQUIRE_ERRNO(null_errno, mlock(NULL, page) == -1);

	errno = 0;
	ATF_REQUIRE_ERRNO(null_errno, mlock((char *)0, page) == -1);

	errno = 0;
	ATF_REQUIRE_ERRNO(EINVAL, mlock((char *)-1, page) == -1);

	errno = 0;
	ATF_REQUIRE_ERRNO(null_errno, munlock(NULL, page) == -1);

	errno = 0;
	ATF_REQUIRE_ERRNO(null_errno, munlock((char *)0, page) == -1);

	errno = 0;
	ATF_REQUIRE_ERRNO(EINVAL, munlock((char *)-1, page) == -1);

	/*
	 * Try to create a pointer to an unmapped page - first after current
	 * brk will likely do.
	 */
	invalid_ptr = (void*)(((uintptr_t)sbrk(0)+page) & ~(page-1));
	printf("testing with (hopefully) invalid pointer %p\n", invalid_ptr);

	errno = 0;
	ATF_REQUIRE_ERRNO(ENOMEM, mlock(invalid_ptr, page) == -1);

	errno = 0;
	ATF_REQUIRE_ERRNO(ENOMEM, munlock(invalid_ptr, page) == -1);
}

ATF_TC(mlock_limits);
ATF_TC_HEAD(mlock_limits, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test system limits with mlock(2)");
}

ATF_TC_BODY(mlock_limits, tc)
{
	struct rlimit res;
	void *buf;
	pid_t pid;
	int sta;

	buf = malloc(page);
	ATF_REQUIRE(buf != NULL);

	pid = fork();
	ATF_REQUIRE(pid >= 0);

	if (pid == 0) {

		for (ssize_t i = page; i >= 2; i -= 100) {

			res.rlim_cur = i - 1;
			res.rlim_max = i - 1;

			(void)fprintf(stderr, "trying to lock %zd bytes "
			    "with %zu byte limit\n", i, (size_t)res.rlim_cur);

			if (setrlimit(RLIMIT_MEMLOCK, &res) != 0)
				_exit(EXIT_FAILURE);

			errno = 0;

			if (mlock(buf, i) != -1 || errno != EAGAIN) {
				(void)munlock(buf, i);
				_exit(EXIT_FAILURE);
			}
		}

		_exit(EXIT_SUCCESS);
	}

	(void)wait(&sta);

	if (WIFEXITED(sta) == 0 || WEXITSTATUS(sta) != EXIT_SUCCESS)
		atf_tc_fail("mlock(2) locked beyond system limits");

	free(buf);
}

ATF_TC(mlock_mmap);
ATF_TC_HEAD(mlock_mmap, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mlock(2)-mmap(2) interaction");
}

ATF_TC_BODY(mlock_mmap, tc)
{
	static const int flags = MAP_ANON | MAP_PRIVATE | MAP_WIRED;
	void *buf;

	/*
	 * Make a wired RW mapping and check that mlock(2)
	 * does not fail for the (already locked) mapping.
	 */
	buf = mmap(NULL, page, PROT_READ | PROT_WRITE, flags, -1, 0);

	ATF_REQUIRE(buf != MAP_FAILED);
	ATF_REQUIRE(mlock(buf, page) == 0);
	ATF_REQUIRE(munlock(buf, page) == 0);
	ATF_REQUIRE(munmap(buf, page) == 0);
	ATF_REQUIRE(munlock(buf, page) != 0);

	/*
	 * But it should be impossible to mlock(2) a PROT_NONE mapping.
	 */
	buf = mmap(NULL, page, PROT_NONE, flags, -1, 0);

	ATF_REQUIRE(buf != MAP_FAILED);
	ATF_REQUIRE(mlock(buf, page) != 0);
	ATF_REQUIRE(munmap(buf, page) == 0);
}

ATF_TC(mlock_nested);
ATF_TC_HEAD(mlock_nested, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that consecutive mlock(2) calls succeed");
}

ATF_TC_BODY(mlock_nested, tc)
{
	const size_t maxiter = 100;
	void *buf;

	buf = malloc(page);
	ATF_REQUIRE(buf != NULL);

	for (size_t i = 0; i < maxiter; i++)
		ATF_REQUIRE(mlock(buf, page) == 0);

	ATF_REQUIRE(munlock(buf, page) == 0);
	free(buf);
}

ATF_TP_ADD_TCS(tp)
{

	page = sysconf(_SC_PAGESIZE);
	ATF_REQUIRE(page >= 0);

	ATF_TP_ADD_TC(tp, mlock_clip);
	ATF_TP_ADD_TC(tp, mlock_err);
	ATF_TP_ADD_TC(tp, mlock_limits);
	ATF_TP_ADD_TC(tp, mlock_mmap);
	ATF_TP_ADD_TC(tp, mlock_nested);

	return atf_no_error();
}
