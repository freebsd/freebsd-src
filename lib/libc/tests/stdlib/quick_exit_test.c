/*-
 * Copyright (c) 2023 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

static void func_a(void)
{
	if (write(STDOUT_FILENO, "a", 1) != 1)
		_Exit(1);
}

static void func_b(void)
{
	if (write(STDOUT_FILENO, "b", 1) != 1)
		_Exit(1);
}

static void func_c(void)
{
	if (write(STDOUT_FILENO, "c", 1) != 1)
		_Exit(1);
}

static void child(void)
{
	// this will be received by the parent
	printf("hello, ");
	fflush(stdout);
	// this won't, because quick_exit() does not flush
	printf("world");
	// these will be called in reverse order, producing "abc"
	if (at_quick_exit(func_c) != 0 ||
	    at_quick_exit(func_b) != 0 ||
	    at_quick_exit(func_a) != 0)
		_Exit(1);
	quick_exit(0);
}

ATF_TC_WITHOUT_HEAD(quick_exit);
ATF_TC_BODY(quick_exit, tc)
{
	char buf[100] = "";
	ssize_t len;
	int p[2], wstatus = 0;
	pid_t pid;

	ATF_REQUIRE(pipe(p) == 0);
	pid = fork();
	if (pid == 0) {
		if (dup2(p[1], STDOUT_FILENO) < 0)
			_Exit(1);
		(void)close(p[1]);
		(void)close(p[0]);
		child();
		_Exit(1);
	}
	ATF_REQUIRE_MSG(pid > 0,
	    "expect fork() to succeed");
	ATF_CHECK_EQ_MSG(pid, waitpid(pid, &wstatus, 0),
	    "expect to collect child process");
	ATF_CHECK_EQ_MSG(0, wstatus,
	    "expect child to exit cleanly");
	ATF_CHECK_MSG((len = read(p[0], buf, sizeof(buf))) > 0,
	    "expect to receive output from child");
	ATF_CHECK_STREQ("hello, abc", buf);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, quick_exit);
	return (atf_no_error());
}
