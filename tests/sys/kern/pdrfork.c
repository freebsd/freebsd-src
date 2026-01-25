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
#include <sys/user.h>
#include <sys/procdesc.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void basic_usage(int rfflags) {
	int pd = -1;
	pid_t pid, pd_pid, waited_pid;
	int r, status;

	pid = pdrfork(&pd, 0, rfflags);
	ATF_REQUIRE_MSG(pid >= 0, "rfork failed with %s", strerror(errno));
	if (pid == 0) {
		/* In child */
		_exit(0);
	}
	ATF_REQUIRE_MSG(pd >= 0, "rfork did not return a process descriptor");
	r = pdgetpid(pd, &pd_pid);
	ATF_CHECK_EQ_MSG(r, 0, "pdgetpid failed: %s", strerror(errno));

	/* We should be able to collect the child's status */
	waited_pid = waitpid(pid, &status, WEXITED | WNOWAIT);
	ATF_CHECK_EQ(waited_pid, pid);

	/* But after closing the process descriptor, we won't */
	close(pd);
	waited_pid = waitpid(pid, &status, WEXITED | WNOHANG);
	ATF_CHECK_EQ(-1, waited_pid);
	ATF_CHECK_EQ(ECHILD, errno);
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

	waited_pid = waitpid(pid, &status, WEXITED | WNOWAIT);
	ATF_CHECK_EQ(waited_pid, pid);
	ATF_REQUIRE(WIFEXITED(status) && (WEXITSTATUS(status) == true));

	close(pd);
}

/* If the pidfd argument is invalid, the error should be handled gracefully */
ATF_TC_WITHOUT_HEAD(efault);
ATF_TC_BODY(efault, tc)
{
	ATF_REQUIRE_ERRNO(EFAULT, pdrfork((int*)-1, 0, RFPROC | RFPROCDESC) < 0);
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
}

/*
 * Without RFSPAWN, RFPROC, or RFPROCDESC, an existing process may be modified
 */
ATF_TC_WITHOUT_HEAD(modify_child);
ATF_TC_BODY(modify_child, tc)
{
	int fdp = -1;
	pid_t pid1, pid2;

	pid1 = pdfork(&fdp, 0);
	if (pid1 == 0)
		_exit(0);
	ATF_REQUIRE_MSG(pid1 >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	pid2 = pdrfork(&fdp, 0, RFNOWAIT);
	ATF_REQUIRE_MSG(pid2 >= 0, "pdrfork failed: %s", strerror(errno));
	ATF_CHECK_EQ_MSG(pid2, 0,
	    "pdrfork created a process even though we told it not to");

	close(fdp);
}

/*
 * Basic usage with RFPROC.  No process descriptor will be created.
 * I'm not sure why you would use pdrfork in this case instead of plain rfork
 */
ATF_TC_WITHOUT_HEAD(rfproc);
ATF_TC_BODY(rfproc, tc)
{
	int pd = -1;
	pid_t pid;

	pid = pdrfork(&pd, 0, RFPROC);
	ATF_REQUIRE_MSG(pid > 0, "rfork failed with %s", strerror(errno));
	if (pid == 0)
		_exit(0);

	ATF_REQUIRE_EQ_MSG(pd, -1,
	    "rfork(RFPROC) returned a process descriptor");
}

/* basic usage with RFPROCDESC */
ATF_TC_WITHOUT_HEAD(rfprocdesc);
ATF_TC_BODY(rfprocdesc, tc)
{
	basic_usage(RFPROC | RFPROCDESC);
}

/* basic usage with RFSPAWN */
/*
 * Skip on i386 and x86_64 because RFSPAWN cannot be used from C code on those
 * architectures.  See lib/libc/gen/posix_spawn.c for details.
 */
#if !(defined(__i386__)) && !(defined(__amd64__))
ATF_TC_WITHOUT_HEAD(rfspawn);
ATF_TC_BODY(rfspawn, tc)
{
	basic_usage(RFSPAWN);
}
#endif

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, child_gets_no_pidfd);
	ATF_TP_ADD_TC(tp, efault);
	ATF_TP_ADD_TC(tp, einval);
	ATF_TP_ADD_TC(tp, modify_child);
	ATF_TP_ADD_TC(tp, rfproc);
	ATF_TP_ADD_TC(tp, rfprocdesc);
#if !(defined(__i386__)) && !(defined(__amd64__))
	ATF_TP_ADD_TC(tp, rfspawn);
#endif

	return (atf_no_error());
}
