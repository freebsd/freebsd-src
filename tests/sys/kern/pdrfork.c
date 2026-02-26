/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 ConnectWise
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
#include <sys/procdesc.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void
basic_usage_tail(int pd, pid_t pid)
{
	pid_t pd_pid, waited_pid;
	int r, status;

	ATF_REQUIRE_MSG(pd >= 0, "rfork did not return a process descriptor");
	r = pdgetpid(pd, &pd_pid);
	ATF_CHECK_EQ_MSG(r, 0, "pdgetpid failed: %s", strerror(errno));
	ATF_CHECK_EQ(pd_pid, pid);

	/* We should be able to collect the child's status */
	waited_pid = waitpid(pid, &status, WEXITED);
	ATF_CHECK_EQ(waited_pid, pid);

	/* But after closing the process descriptor, we won't */
	close(pd);
	waited_pid = waitpid(pid, &status, WEXITED | WNOHANG);
	ATF_CHECK_EQ(-1, waited_pid);
	ATF_CHECK_EQ(ECHILD, errno);
}

static void
basic_usage(int rfflags)
{
	int pd = -1;
	pid_t pid;

	pid = pdrfork(&pd, 0, rfflags);
	ATF_REQUIRE_MSG(pid >= 0, "rfork failed with %s", strerror(errno));
	if (pid == 0) {
		/* In child */
		_exit(0);
	}
	basic_usage_tail(pd, pid);
}

/* pdrfork does not return a process descriptor to the child */
ATF_TC_WITHOUT_HEAD(child_gets_no_pidfd);
ATF_TC_BODY(child_gets_no_pidfd, tc)
{
	int pd = -1;
	pid_t pid, pd_pid, waited_pid;
	int r, status;

	pid = pdrfork(&pd, 0, RFPROC | RFPROCDESC);
	ATF_REQUIRE_MSG(pid >= 0, "rfork failed with %s", strerror(errno));
	if (pid == 0) {
		/*
		 * In child.  We can't do very much here before we exec, so
		 * just use our exit status to report success.
		 */
		_exit(pd == -1);
	}
	ATF_REQUIRE_MSG(pd >= 0, "rfork did not return a process descriptor");
	r = pdgetpid(pd, &pd_pid);
	ATF_CHECK_EQ_MSG(r, 0, "pdgetpid failed: %s", strerror(errno));

	waited_pid = waitpid(pid, &status, WEXITED);
	ATF_CHECK_EQ(waited_pid, pid);
	ATF_REQUIRE(WIFEXITED(status) && (WEXITSTATUS(status) == true));

	close(pd);
}

/* If the pidfd argument is invalid, the error should be handled gracefully */
ATF_TC_WITHOUT_HEAD(efault);
ATF_TC_BODY(efault, tc)
{
	void *unmapped;
	pid_t my_pid;

	unmapped = mmap(NULL, PAGE_SIZE, PROT_NONE, MAP_GUARD, -1, 0);
	ATF_REQUIRE(unmapped != MAP_FAILED);
	my_pid = getpid();
	ATF_REQUIRE_ERRNO(EFAULT, pdrfork(unmapped, 0, RFPROC |
	    RFPROCDESC) < 0);

	/*
	 * EFAULT only means that the copyout of the procdesc failed.
	 * The runaway child was created anyway.  Prevent
	 * double-destruction of the atf stuff.
	 */
	if (my_pid != getpid())
		_exit(0);
}

/* Invalid combinations of flags should return EINVAL */
ATF_TC_WITHOUT_HEAD(einval);
ATF_TC_BODY(einval, tc)
{
	int pd = -1;

	ATF_CHECK_ERRNO(EINVAL, pdrfork(&pd, -1, RFSPAWN) < 0);
	ATF_CHECK_ERRNO(EINVAL, pdrfork(&pd, 0, -1) < 0);
	ATF_CHECK_ERRNO(EINVAL, pdrfork(&pd, 0, RFSPAWN | RFNOWAIT) < 0);
	ATF_CHECK_ERRNO(EINVAL, pdrfork(&pd, 0, RFPROC | RFFDG| RFCFDG) < 0);
	ATF_CHECK_ERRNO(EINVAL, pdrfork(&pd, 0, RFPROCDESC) < 0);
	ATF_CHECK_ERRNO(EINVAL, pdrfork(&pd, 0, RFPROC) < 0);
	ATF_CHECK_ERRNO(EINVAL, pdrfork(&pd, 0, 0) < 0);
}

/* basic usage with RFPROCDESC */
ATF_TC_WITHOUT_HEAD(rfprocdesc);
ATF_TC_BODY(rfprocdesc, tc)
{
	basic_usage(RFPROC | RFPROCDESC);
}

static int
rfspawn_fn(void *arg)
{
	_exit(0);
	return (0);
}

/* basic usage with RFSPAWN */
ATF_TC_WITHOUT_HEAD(rfspawn);
ATF_TC_BODY(rfspawn, tc)
{
	char *stack = NULL;
	int pd = -1;
	pid_t pid;

#if defined(__i386__) || defined(__amd64__)
#define STACK_SZ	(PAGE_SIZE * 10)
	stack = mmap(NULL, STACK_SZ, PROT_READ | PROT_WRITE, MAP_ANON,
	    -1, 0);
	ATF_REQUIRE(stack != MAP_FAILED);
	stack += STACK_SZ;
#endif
	pid = pdrfork_thread(&pd, 0, RFSPAWN, stack, rfspawn_fn, NULL);
	basic_usage_tail(pd, pid);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, child_gets_no_pidfd);
	ATF_TP_ADD_TC(tp, efault);
	ATF_TP_ADD_TC(tp, einval);
	ATF_TP_ADD_TC(tp, rfprocdesc);
	ATF_TP_ADD_TC(tp, rfspawn);

	return (atf_no_error());
}
