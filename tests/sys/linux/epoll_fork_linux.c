/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#include "linux_syscalls.h"
#include "linux_test.h"

static int failed;

static void
check(const char *description, long actual, long expected)
{

	if (actual == expected)
		return;
	putstr("FAIL: ");
	putstr(description);
	putstr("\n");
	failed++;
}

static void
test_flags(int flags)
{
	struct linux_epoll_event event = { 0 }, output = { 0 };
	int epfd, extra[2], pipefd[2], status = 0;
	linux_pid_t child, result;

	event.events = LINUX_EPOLLIN;
	event.data = 0x1234;
	epfd = setup(epoll_create1(flags), "epoll_create1", &failed);
	if (epfd < 0)
		return;
	if (setup(pipe2(pipefd, 0), "pipe2", &failed) < 0)
		return;
	if (setup(epoll_ctl(epfd, LINUX_EPOLL_CTL_ADD, pipefd[0],
	    &event), "epoll_ctl(ADD)", &failed) < 0)
		return;
	if (setup(write(pipefd[1], "x", 1), "write", &failed) < 0)
		return;

	child = clone(LINUX_SIGCHLD, 0, 0, 0, 0);
	if (child < 0) {
		(void)setup(child, "fork", &failed);
		return;
	}
	if (child == 0) {
		failed = 0;
		check("epoll F_GETFD", fcntl(epfd, LINUX_F_GETFD, 0),
		    flags != 0 ? 1 : 0);
		check("pipe F_GETFD", fcntl(pipefd[0], LINUX_F_GETFD, 0),
		    0);
		check("inherited readiness",
		    epoll_pwait(epfd, &output, 1, 0, 0, 0), 1);
		check("inherited event data", output.data, event.data);
		if (setup(pipe2(extra, 0), "child pipe2", &failed) >= 0)
			check("epoll_ctl after fd allocation", epoll_ctl(epfd,
			    LINUX_EPOLL_CTL_MOD, pipefd[0], &event), 0);
		exit_group(failed != 0 ? 1 : 0);
	}

	do {
		result = wait4(child, &status, 0, 0);
	} while (result == -1 && errno == LINUX_EINTR);
	check("waitpid", result, child);
	check("child status", status, 0);
	(void)close(epfd);
	(void)close(pipefd[0]);
	(void)close(pipefd[1]);
}

int
main(void)
{

	test_flags(0);
	test_flags(LINUX_O_CLOEXEC);
	return (failed != 0 ? 1 : 0);
}
