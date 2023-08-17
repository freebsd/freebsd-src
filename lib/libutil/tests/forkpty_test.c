/*-
 * Copyright (c) 2023 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/resource.h>
#include <sys/wait.h>

#include <errno.h>
#include <libutil.h>
#include <unistd.h>

#include <atf-c.h>

ATF_TC(forkfail);
ATF_TC_HEAD(forkfail, tc)
{
	atf_tc_set_md_var(tc, "descr", "Check for fd leak when fork() fails");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}

ATF_TC_BODY(forkfail, tc)
{
	struct rlimit orl, nrl;
	pid_t pid;
	int prevfd, fd, pty;

	/* set process limit to 1 so fork() will fail */
	ATF_REQUIRE_EQ(0, getrlimit(RLIMIT_NPROC, &orl));
	nrl = orl;
	nrl.rlim_cur = 1;
	ATF_REQUIRE_EQ(0, setrlimit(RLIMIT_NPROC, &nrl));
	/* check first free fd */
	ATF_REQUIRE((fd = dup(0)) > 0);
	ATF_REQUIRE_EQ(0, close(fd));
	/* attempt forkpty() */
	pid = forkpty(&pty, NULL, NULL, NULL);
	if (pid == 0) {
		/* child - fork() unexpectedly succeeded */
		_exit(0);
	}
	ATF_CHECK_ERRNO(EAGAIN, pid < 0);
	if (pid > 0) {
		/* parent - fork() unexpectedly succeeded */
		(void)waitpid(pid, NULL, 0);
	}
	/* check that first free fd hasn't changed */
	prevfd = fd;
	ATF_REQUIRE((fd = dup(0)) > 0);
	ATF_CHECK_EQ(prevfd, fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, forkfail);
	return (atf_no_error());
}
