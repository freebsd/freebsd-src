/*-
 * Copyright (c) 2024 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * These tests demonstrate a bug in ppoll() and pselect() where a blocked
 * signal can fire after the timer runs out but before the signal mask is
 * restored.  To do this, we fork a child process which installs a SIGINT
 * handler and repeatedly calls either ppoll() or pselect() with a 1 ms
 * timeout, while the parent repeatedly sends SIGINT to the child at
 * intervals that start out at 1100 us and gradually decrease to 900 us.
 * Each SIGINT resynchronizes parent and child, and sooner or later the
 * parent hits the sweet spot and the SIGINT arrives at just the right
 * time to demonstrate the bug.
 */

#include <sys/select.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

static volatile sig_atomic_t caught[NSIG];

static void
handler(int signo)
{
	caught[signo]++;
}

static void
child(int rd, bool poll)
{
	struct timespec timeout = { .tv_nsec = 1000000 };
	sigset_t set0, set1;
	int ret;

	/* empty mask for ppoll() / pselect() */
	sigemptyset(&set0);

	/* block SIGINT, then install a handler for it */
	sigemptyset(&set1);
	sigaddset(&set1, SIGINT);
	sigprocmask(SIG_BLOCK, &set1, NULL);
	signal(SIGINT, handler);

	/* signal parent that we are ready */
	close(rd);
	for (;;) {
		/* sleep for 1 ms with signals unblocked */
		ret = poll ? ppoll(NULL, 0, &timeout, &set0) :
		    pselect(0, NULL, NULL, NULL, &timeout, &set0);
		/*
		 * At this point, either ret == 0 (timer ran out) errno ==
		 * EINTR (a signal was received).  Any other outcome is
		 * abnormal.
		 */
		if (ret != 0 && errno != EINTR)
			err(1, "p%s()", poll ? "poll" : "select");
		/* if ret == 0, we should not have caught any signals */
		if (ret == 0 && caught[SIGINT]) {
			/*
			 * We successfully demonstrated the race.  Restore
			 * the default action and re-raise SIGINT.
			 */
			signal(SIGINT, SIG_DFL);
			raise(SIGINT);
			/* Not reached */
		}
		/* reset for next attempt */
		caught[SIGINT] = 0;
	}
	/* Not reached */
}

static void
prace(bool poll)
{
	int pd[2], status;
	pid_t pid;

	/* fork child process */
	if (pipe(pd) != 0)
		err(1, "pipe()");
	if ((pid = fork()) < 0)
		err(1, "fork()");
	if (pid == 0) {
		close(pd[0]);
		child(pd[1], poll);
		/* Not reached */
	}
	close(pd[1]);

	/* wait for child to signal readiness */
	(void)read(pd[0], &pd[0], sizeof(pd[0]));
	close(pd[0]);

	/* repeatedly attempt to signal at just the right moment */
	for (useconds_t timeout = 1100; timeout > 900; timeout--) {
		usleep(timeout);
		if (kill(pid, SIGINT) != 0) {
			if (errno != ENOENT)
				err(1, "kill()");
			/* ENOENT means the child has terminated */
			break;
		}
	}

	/* we're done, kill the child for sure */
	(void)kill(pid, SIGKILL);
	if (waitpid(pid, &status, 0) < 0)
		err(1, "waitpid()");

	/* assert that the child died of SIGKILL */
	ATF_REQUIRE(WIFSIGNALED(status));
	ATF_REQUIRE_MSG(WTERMSIG(status) == SIGKILL,
	    "child caught SIG%s", sys_signame[WTERMSIG(status)]);
}

ATF_TC_WITHOUT_HEAD(ppoll_race);
ATF_TC_BODY(ppoll_race, tc)
{
	prace(true);
}

ATF_TC_WITHOUT_HEAD(pselect_race);
ATF_TC_BODY(pselect_race, tc)
{
	prace(false);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ppoll_race);
	ATF_TP_ADD_TC(tp, pselect_race);
	return (atf_no_error());
}
