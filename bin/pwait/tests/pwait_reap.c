/*-
 * Copyright (c) 2026 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * Wait for a process to terminate, optionally reap it, and verify that it
 * exited cleanly.
 */
static void
wait_zero(pid_t pid, bool reap)
{
	int wstatus;
	pid_t ret;

	do {
		ret = waitpid(pid, &wstatus, reap ? 0 : WNOWAIT);
		ATF_REQUIRE(ret == pid || (ret < 0 && errno == EINTR));
	} while (ret != pid);
	ATF_CHECK(WIFEXITED(wstatus));
	ATF_CHECK_EQ(WEXITSTATUS(wstatus), 0);
}

/*
 * Verify that pwait is still waiting for the target process.
 */
static void
pwait_is_waiting(pid_t pwpid, pid_t tpid, int pwout, int pwerr)
{
	struct pollfd pfds[2];
	char line[64] = "", *eol, *rest;
	ssize_t ret;

	pfds[0].fd = pwout;
	pfds[0].events = POLLIN;
	pfds[1].fd = pwerr;
	pfds[1].events = POLLIN;

	/* loop until we have some output */
	do {
		/* send SIGINFO so pwait prints the target PID on stderr */
		ATF_REQUIRE(kill(pwpid, SIGINFO) == 0);
	} while ((ret = poll(pfds, 2, 100)) == 0);
	ATF_REQUIRE_EQ(ret, 1);
	ATF_REQUIRE_EQ(pfds[1].revents, POLLIN);

	/* read line from stderr */
	ATF_REQUIRE((ret = read(pwerr, line, sizeof(line) - 1)) > 0);

	/* verify the line */
	ATF_REQUIRE((eol = strchr(line, '\n')) != NULL);
	*eol = '\0';
	ATF_REQUIRE_EQ(strtol(line, &rest, 10), tpid);
	ATF_REQUIRE_STREQ(rest, "");
}

/*
 * Verify that pwait has reported that the target process has terminated,
 * then reap it.
 */
static void
pwait_is_done(pid_t pwpid, pid_t tpid, int pwout, int pwerr)
{
	struct pollfd pfds[2];
	char line[64] = "", *eol, *rest;
	ssize_t ret;

	pfds[0].fd = pwout;
	pfds[0].events = POLLIN;
	pfds[1].fd = pwerr;
	pfds[1].events = POLLIN;

	/* loop until we have some output */
	do {
		/* nothing */
	} while ((ret = poll(pfds, 2, 100)) == 0);
	ATF_REQUIRE_EQ(ret, 1);
	ATF_REQUIRE_EQ(pfds[0].revents, POLLIN);

	/* read line from stdout */
	ATF_REQUIRE((ret = read(pwout, line, sizeof(line) - 1)) > 0);

	/* verify the line */
	ATF_REQUIRE((eol = strchr(line, '\n')) != NULL);
	*eol = '\0';
	ATF_REQUIRE_EQ(strtol(line, &rest, 10), tpid);
	ATF_REQUIRE_STREQ(rest, ": exited with status 0.");

	/* reap pwait */
	wait_zero(pwpid, true);
}

static void
pwait_reap_test(bool reap)
{
	char pidstr[16];
	pid_t tpid, pwpid;
	int tpipe[2], pwout[2], pwerr[2];

	/* fork target process */
	ATF_REQUIRE(pipe2(tpipe, O_CLOEXEC) == 0);
	ATF_REQUIRE((tpid = fork()) >= 0);
	if (tpid == 0) {
		(void)close(tpipe[1]);
		/* block until parent closes its end of the pipe */
		(void)read(tpipe[0], pidstr, 1);
		_exit(0);
	}
	(void)close(tpipe[0]);

	/* fork pwait */
	snprintf(pidstr, sizeof(pidstr), "%ld", (long)tpid);
	ATF_REQUIRE(pipe2(pwout, O_CLOEXEC) == 0);
	ATF_REQUIRE(pipe2(pwerr, O_CLOEXEC) == 0);
	ATF_REQUIRE((pwpid = fork()) >= 0);
	if (pwpid == 0) {
		(void)dup2(pwout[0], STDOUT_FILENO);
		(void)dup2(pwerr[0], STDERR_FILENO);
		execlp("pwait", "pwait", reap ? "-rv" : "-v", pidstr, NULL);
		_exit(99);
	}

	/* wait for pwait to become ready */
	pwait_is_waiting(pwpid, tpid, pwout[1], pwerr[1]);

	/* unblock the target and wait for it to terminate */
	(void)close(tpipe[1]);
	wait_zero(tpid, false);

	if (reap) {
		/* check that pwait is still waiting */
		pwait_is_waiting(pwpid, tpid, pwout[1], pwerr[1]);
	} else {
		/* check that pwait has reported completion */
		pwait_is_done(pwpid, tpid, pwout[1], pwerr[1]);
	}

	/* reap the target */
	wait_zero(tpid, true);

	if (reap) {
		/* check that pwait has reported completion */
		pwait_is_done(pwpid, tpid, pwout[1], pwerr[1]);
	}
}

ATF_TC(pwait_normal);
ATF_TC_HEAD(pwait_normal, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test that pwait returns before the "
	    "target process has been reaped");
}
ATF_TC_BODY(pwait_normal, tc)
{
	pwait_reap_test(false);
}

ATF_TC(pwait_reap);
ATF_TC_HEAD(pwait_reap, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test that pwait -r does not return "
	    "until the target process has been reaped");
}
ATF_TC_BODY(pwait_reap, tc)
{
	pwait_reap_test(true);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pwait_normal);
	ATF_TP_ADD_TC(tp, pwait_reap);
	return (atf_no_error());
}
