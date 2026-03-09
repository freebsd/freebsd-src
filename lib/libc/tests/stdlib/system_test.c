/*-
 * Copyright (c) 2026 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

ATF_TC(system_true);
ATF_TC_HEAD(system_true, tc)
{
	atf_tc_set_md_var(tc, "descr", "system(\"true\")");
}
ATF_TC_BODY(system_true, tc)
{
	ATF_REQUIRE_EQ(W_EXITCODE(0, 0), system("true"));
}

ATF_TC(system_false);
ATF_TC_HEAD(system_false, tc)
{
	atf_tc_set_md_var(tc, "descr", "system(\"false\")");
}
ATF_TC_BODY(system_false, tc)
{
	ATF_REQUIRE_EQ(W_EXITCODE(1, 0), system("false"));
}

ATF_TC(system_touch);
ATF_TC_HEAD(system_touch, tc)
{
	atf_tc_set_md_var(tc, "descr", "system(\"touch file\")");
}
ATF_TC_BODY(system_touch, tc)
{
	/* The file does not exist */
	ATF_CHECK_ERRNO(ENOENT, unlink("file"));

	/* Run a command that creates it */
	ATF_REQUIRE_EQ(W_EXITCODE(0, 0), system("touch file"));

	/* Now the file exists */
	ATF_CHECK_EQ(0, unlink("file"));
}

ATF_TC(system_null);
ATF_TC_HEAD(system_null, tc)
{
	atf_tc_set_md_var(tc, "descr", "system(NULL)");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(system_null, tc)
{
	/* First, test in a normal environment */
	ATF_REQUIRE_EQ(1, system(NULL));

	/* Now enter an empty chroot */
	ATF_REQUIRE_EQ(0, chroot("."));
	ATF_REQUIRE_EQ(0, chdir("/"));

	/* Test again with no shell available */
	ATF_REQUIRE_EQ(0, system(NULL));
	ATF_REQUIRE_EQ(W_EXITCODE(127, 0), system("true"));
}

/*
 * Define PROCMASK_IS_THREADMASK if sigprocmask() gets / sets the thread
 * mask in multithreaded programs, which makes it impossible to verify
 * that system(3) correctly blocks and unblocks SIGCHLD.
 */
#ifdef __FreeBSD__
#define PROCMASK_IS_THREADMASK 1
#endif

static void *
system_thread(void *arg)
{
	char cmd[64];
	int i = (int)(intptr_t)arg;

	snprintf(cmd, sizeof(cmd), "rm flag%d ; lockf -ns lock%d true", i, i);
	return ((void *)(intptr_t)system(cmd));
}

static inline int
sigcmpset(const sigset_t *a, const sigset_t *b)
{
	return (memcmp(a, b, sizeof(sigset_t)));
}

ATF_TC(system_concurrent);
ATF_TC_HEAD(system_concurrent, tc)
{
	atf_tc_set_md_var(tc, "descr", "Concurrent calls");
}
ATF_TC_BODY(system_concurrent, tc)
{
	enum { N = 3 };
	struct sigaction sigint, sigquit, sigact;
	sigset_t normset, sigset;
	pthread_t thr[N];
	char fn[8];
	int fd[N];
	void *arg, *ret;

	/* Create and lock the locks */
	for (int i = 0; i < N; i++) {
		snprintf(fn, sizeof(fn), "lock%d", i);
		fd[i] = open(fn, O_CREAT|O_EXCL|O_EXLOCK|O_CLOEXEC, 0644);
		ATF_REQUIRE_MSG(fd[i] >= 0, "%s: %m", fn);
	}

	/* Create the flags */
	for (int i = 0; i < N; i++) {
		snprintf(fn, sizeof(fn), "flag%d", i);
		ATF_REQUIRE_EQ(0, symlink(fn, fn));
	}

	/* Save the current signal dispositions */
	ATF_REQUIRE_EQ(0, sigaction(SIGINT, NULL, &sigint));
	ATF_REQUIRE_EQ(0, sigaction(SIGQUIT, NULL, &sigquit));
	ATF_REQUIRE_EQ(0, sigprocmask(0, NULL, &normset));

	/* Spawn threads which block on these files */
	for (int i = 0; i < N; i++) {
		arg = (void *)(intptr_t)i;
		ATF_REQUIRE_INTEQ(0,
		    pthread_create(&thr[i], NULL, system_thread, arg));
	}

	/* Wait until the flags are gone */
	for (int i = 0; i < N; i++) {
		snprintf(fn, sizeof(fn), "flag%d", i);
		while (readlink(fn, fn, sizeof(fn)) > 0)
			usleep(10000);
		ATF_REQUIRE_EQ(ENOENT, errno);
	}

	/* Release the locks */
	for (int i = 0; i < N; i++) {
		/* Check the signal dispositions */
		ATF_REQUIRE_EQ(0, sigaction(SIGINT, NULL, &sigact));
		ATF_CHECK_EQ(SIG_IGN, sigact.sa_handler);
		ATF_REQUIRE_EQ(0, sigaction(SIGQUIT, NULL, &sigact));
		ATF_CHECK_EQ(SIG_IGN, sigact.sa_handler);
#ifndef PROCMASK_IS_THREADMASK
		ATF_REQUIRE_EQ(0, sigprocmask(0, NULL, &sigset));
		ATF_CHECK(sigismember(&sigset, SIGCHLD));
#endif

		/* Close the file, releasing the lock */
		ATF_REQUIRE_INTEQ(0, close(fd[i]));

		/* Join the thread and check the return value */
		ATF_CHECK_INTEQ(0, pthread_join(thr[i], &ret));
		ATF_CHECK_INTEQ(W_EXITCODE(0, 0), (int)(intptr_t)ret);
	}

	/* Check the signal dispositions */
	ATF_REQUIRE_EQ(0, sigaction(SIGINT, NULL, &sigact));
	ATF_CHECK_EQ(sigint.sa_handler, sigact.sa_handler);
	ATF_CHECK_EQ(sigint.sa_flags, sigact.sa_flags);
	ATF_CHECK_EQ(0, sigcmpset(&sigint.sa_mask, &sigact.sa_mask));
	ATF_REQUIRE_EQ(0, sigaction(SIGQUIT, NULL, &sigact));
	ATF_CHECK_EQ(sigquit.sa_handler, sigact.sa_handler);
	ATF_CHECK_EQ(sigquit.sa_flags, sigact.sa_flags);
	ATF_CHECK_EQ(0, sigcmpset(&sigquit.sa_mask, &sigact.sa_mask));
	ATF_REQUIRE_EQ(0, sigprocmask(0, NULL, &sigset));
	ATF_CHECK_EQ(0, sigcmpset(&sigset, &normset));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, system_true);
	ATF_TP_ADD_TC(tp, system_false);
	ATF_TP_ADD_TC(tp, system_touch);
	ATF_TP_ADD_TC(tp, system_null);
	ATF_TP_ADD_TC(tp, system_concurrent);
	return (atf_no_error());
}
