/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Maxim Konovalov <maxim@FreeBSD.org>
 */

/*
 * fdcopy() does not copy some descriptors into the child (plain kqueues,
 * close-on-fork descriptors).  The child's lowest-free-descriptor hint
 * must still let dup(2) return the lowest unused descriptor, as POSIX
 * requires.  See PR 300234.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include <atf-c.h>

#define	RFD	100
#define	WFD	101

/* Fork and return the descriptor the child gets from dup(0). */
static int
child_dup(void)
{
	int fds[2], fd, status;
	pid_t pid;

	/* Park the pipe high so that it does not change the layout. */
	ATF_REQUIRE(pipe(fds) == 0);
	ATF_REQUIRE(dup2(fds[0], RFD) == RFD);
	ATF_REQUIRE(dup2(fds[1], WFD) == WFD);
	ATF_REQUIRE(close(fds[0]) == 0);
	ATF_REQUIRE(close(fds[1]) == 0);

	pid = fork();
	ATF_REQUIRE(pid != -1);
	if (pid == 0) {
		fd = dup(STDIN_FILENO);
		_exit(write(WFD, &fd, sizeof(fd)) == sizeof(fd) ? 0 : 1);
	}
	ATF_REQUIRE(close(WFD) == 0);
	ATF_REQUIRE(read(RFD, &fd, sizeof(fd)) == sizeof(fd));
	ATF_REQUIRE(close(RFD) == 0);
	ATF_REQUIRE(waitpid(pid, &status, 0) == pid);
	ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	return (fd);
}

static int
open_null(int flags)
{
	int fd;

	fd = open("/dev/null", O_RDONLY | flags);
	ATF_REQUIRE(fd != -1);
	return (fd);
}

ATF_TC_WITHOUT_HEAD(kqueue_no_hole);
ATF_TC_BODY(kqueue_no_hole, tc)
{
	int kq;

	kq = kqueue();
	ATF_REQUIRE(kq != -1);
	ATF_CHECK_EQ(child_dup(), kq);
}

ATF_TC_WITHOUT_HEAD(kqueue_two);
ATF_TC_BODY(kqueue_two, tc)
{
	int kq1, kq2;

	kq1 = kqueue();
	ATF_REQUIRE(kq1 != -1);
	kq2 = kqueue();
	ATF_REQUIRE(kq2 != -1);
	ATF_CHECK_EQ(child_dup(), kq1);
}

ATF_TC_WITHOUT_HEAD(kqueue_above_hole);
ATF_TC_BODY(kqueue_above_hole, tc)
{
	int a, b, kq;

	a = open_null(0);
	b = open_null(0);
	kq = kqueue();
	ATF_REQUIRE(kq > b);
	ATF_REQUIRE(close(a) == 0);
	ATF_CHECK_EQ(child_dup(), a);
}

ATF_TC_WITHOUT_HEAD(kqueue_cponfork);
ATF_TC_BODY(kqueue_cponfork, tc)
{
	int kq;

	kq = kqueuex(KQUEUE_CPONFORK);
	ATF_REQUIRE(kq != -1);
	ATF_CHECK_EQ(child_dup(), kq + 1);
}

ATF_TC_WITHOUT_HEAD(clofork_no_hole);
ATF_TC_BODY(clofork_no_hole, tc)
{
	int fd;

	fd = open_null(O_CLOFORK);
	ATF_CHECK_EQ(child_dup(), fd);
}

ATF_TC_WITHOUT_HEAD(clofork_above_hole);
ATF_TC_BODY(clofork_above_hole, tc)
{
	int a, b, c;

	a = open_null(0);
	b = open_null(0);
	c = open_null(O_CLOFORK);
	ATF_REQUIRE(c > b);
	ATF_REQUIRE(close(a) == 0);
	ATF_CHECK_EQ(child_dup(), a);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, kqueue_no_hole);
	ATF_TP_ADD_TC(tp, kqueue_two);
	ATF_TP_ADD_TC(tp, kqueue_above_hole);
	ATF_TP_ADD_TC(tp, kqueue_cponfork);
	ATF_TP_ADD_TC(tp, clofork_no_hole);
	ATF_TP_ADD_TC(tp, clofork_above_hole);

	return (atf_no_error());
}
