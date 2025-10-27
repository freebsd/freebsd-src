/*-
 * Copyright (c) 2009 Hudson River Trading LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
 * All rights reserved.
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

/*
 * Regression tests for the closefrom(2) system call.
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static char *shared_page;

/*
 * A variant of ATF_REQUIRE that is suitable for use in child
 * processes.  Since these tests close stderr, errors are reported to
 * a shared page of memory checked by the parent process.
 */
#define	CHILD_REQUIRE(exp) do {				\
	if (!(exp))					\
		child_fail_require(__FILE__, __LINE__,	\
		    #exp " not met");			\
} while (0)

static __dead2 __printflike(3, 4) void
child_fail_require(const char *file, int line, const char *fmt, ...)
{
	FILE *fp;
	va_list ap;

	fp = fmemopen(shared_page, PAGE_SIZE - 1, "w");
	if (fp == NULL)
		exit(1);

	fprintf(fp, "%s:%d: ", file, line);
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	fclose(fp);

	exit(0);
}

static pid_t
child_fork(void)
{
	shared_page = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON |
	    MAP_SHARED, -1, 0);
	ATF_REQUIRE_MSG(shared_page != MAP_FAILED, "mmap: %s", strerror(errno));
	return (atf_utils_fork());
}

static void
child_wait(pid_t pid)
{
	atf_utils_wait(pid, 0, "", "");
	if (shared_page[0] != '\0')
		atf_tc_fail("%s", shared_page);
}

/*
 * Use kinfo_getfile() to fetch the list of file descriptors and figure out
 * the highest open file descriptor.
 */
static int
highest_fd(void)
{
	struct kinfo_file *kif;
	int cnt, i, highest;

	kif = kinfo_getfile(getpid(), &cnt);
	ATF_REQUIRE_MSG(kif != NULL, "kinfo_getfile: %s", strerror(errno));
	highest = -1;
	for (i = 0; i < cnt; i++)
		if (kif[i].kf_fd > highest)
			highest = kif[i].kf_fd;
	free(kif);
	return (highest);
}

static int
devnull(void)
{
	int fd;

	fd = open(_PATH_DEVNULL, O_RDONLY);
	ATF_REQUIRE_MSG(fd != -1, "open(\" "_PATH_DEVNULL" \"): %s",
	    strerror(errno));
	return (fd);
}

ATF_TC_WITHOUT_HEAD(closefrom_simple);
ATF_TC_BODY(closefrom_simple, tc)
{
	int fd, start;

	/* We'd better start up with fd's 0, 1, and 2 open. */
	start = highest_fd();
	ATF_REQUIRE(start >= 2);

	fd = devnull();
	ATF_REQUIRE(fd > start);

	/* Make sure highest_fd() works. */
	ATF_REQUIRE_INTEQ(fd, highest_fd());

	/* Try to use closefrom() to close just the new fd. */
	closefrom(fd);
	ATF_REQUIRE_INTEQ(start, highest_fd());
}

ATF_TC_WITHOUT_HEAD(closefrom_with_holes);
ATF_TC_BODY(closefrom_with_holes, tc)
{
	int i, start;
	
	start = highest_fd();

	/* Eat up 16 descriptors. */
	for (i = 0; i < 16; i++)
		(void)devnull();

	ATF_REQUIRE_INTEQ(start + 16, highest_fd());

	/* Close half of them. */
	closefrom(start + 9);
	ATF_REQUIRE_INTEQ(start + 8, highest_fd());

	/* Explicitly close two descriptors to create holes. */
	ATF_REQUIRE_MSG(close(start + 3) == 0, "close(start + 3): %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(close(start + 5) == 0, "close(start + 5): %s",
	    strerror(errno));

	/* Verify that close on the closed descriptors fails with EBADF. */
	ATF_REQUIRE_ERRNO(EBADF, close(start + 3) == -1);
	ATF_REQUIRE_ERRNO(EBADF, close(start + 5) == -1);

	/* Close most remaining descriptors. */
	closefrom(start + 2);
	ATF_REQUIRE_INTEQ(start + 1, highest_fd());
}

ATF_TC_WITHOUT_HEAD(closefrom_zero);
ATF_TC_BODY(closefrom_zero, tc)
{
	pid_t pid;
	int fd;

	/* Ensure standard descriptors are open. */
	ATF_REQUIRE(highest_fd() >= 2);

	pid = child_fork();
	if (pid == 0) {
		/* Child. */
		closefrom(0);
		fd = highest_fd();
		CHILD_REQUIRE(fd == -1);
		exit(0);
	}

	child_wait(pid);
}

ATF_TC_WITHOUT_HEAD(closefrom_negative_one);
ATF_TC_BODY(closefrom_negative_one, tc)
{
	pid_t pid;
	int fd;

	/* Ensure standard descriptors are open. */
	ATF_REQUIRE(highest_fd() >= 2);

	pid = child_fork();
	if (pid == 0) {
		/* Child. */
		closefrom(-1);
		fd = highest_fd();
		CHILD_REQUIRE(fd == -1);
		exit(0);
	}

	child_wait(pid);
}

ATF_TC_WITHOUT_HEAD(closefrom_in_holes);
ATF_TC_BODY(closefrom_in_holes, tc)
{
	int start;

	start = highest_fd();
	ATF_REQUIRE(start >= 2);

	/* Dup stdout to a higher fd. */
	ATF_REQUIRE_INTEQ(start + 4, dup2(1, start + 4));
	ATF_REQUIRE_INTEQ(start + 4, highest_fd());

	/* Do a closefrom() starting in a hole. */
	closefrom(start + 2);
	ATF_REQUIRE_INTEQ(start, highest_fd());

	/* Do a closefrom() beyond our highest open fd. */
	closefrom(start + 32);
	ATF_REQUIRE_INTEQ(start, highest_fd());
}

ATF_TC_WITHOUT_HEAD(closerange_basic);
ATF_TC_BODY(closerange_basic, tc)
{
	struct stat sb;
	int i, start;

	start = highest_fd();

	/* Open 8 file descriptors */
	for (i = 0; i < 8; i++)
		(void)devnull();
	ATF_REQUIRE_INTEQ(start + 8, highest_fd());

	/* close_range() a hole in the middle */
	ATF_REQUIRE_INTEQ(0, close_range(start + 3, start + 5, 0));
	for (i = start + 3; i < start + 6; ++i)
		ATF_REQUIRE_ERRNO(EBADF, fstat(i, &sb) == -1);

	/* close_range from the middle of the hole */
	ATF_REQUIRE_INTEQ(0, close_range(start + 4, start + 6, 0));
	ATF_REQUIRE_INTEQ(start + 8, highest_fd());

	/* close_range to the end; effectively closefrom(2) */
	ATF_REQUIRE_INTEQ(0, close_range(start + 3, ~0L, 0));
	ATF_REQUIRE_INTEQ(start + 2, highest_fd());

	/* Now close the rest */
	ATF_REQUIRE_INTEQ(0, close_range(start + 1, start + 4, 0));
	ATF_REQUIRE_INTEQ(start, highest_fd());
}

ATF_TC_WITHOUT_HEAD(closefrom_zero_twice);
ATF_TC_BODY(closefrom_zero_twice, tc)
{
	pid_t pid;
	int fd;

	/* Ensure standard descriptors are open. */
	ATF_REQUIRE(highest_fd() >= 2);

	pid = child_fork();
	if (pid == 0) {
		/* Child. */
		closefrom(0);
		fd = highest_fd();
		CHILD_REQUIRE(fd == -1);
		closefrom(0);
		fd = highest_fd();
		CHILD_REQUIRE(fd == -1);
		exit(0);
	}

	child_wait(pid);
}

static void
require_fd_flag(int fd, const char *descr, const char *descr2, int flag,
    bool set)
{
	int flags;

	flags = fcntl(fd, F_GETFD);
	ATF_REQUIRE_MSG(flags >= 0, "fcntl(.., F_GETFD): %s", strerror(errno));

	if (set) {
		ATF_REQUIRE_MSG((flags & flag) == flag,
		    "%s did not set %s on fd %d", descr, descr2, fd);
	} else {
		ATF_REQUIRE_MSG((flags & flag) == 0,
		    "%s set %s when it should not have on fd %d", descr, descr2,
		    fd);
	}
}

ATF_TC_WITHOUT_HEAD(closerange_CLOEXEC);
ATF_TC_BODY(closerange_CLOEXEC, tc)
{
	int i, start;

	start = highest_fd();
	ATF_REQUIRE(start >= 2);

	for (i = 0; i < 8; i++)
		(void)devnull();
	ATF_REQUIRE_INTEQ(start + 8, highest_fd());

	ATF_REQUIRE_INTEQ(0, close_range(start + 2, start + 5,
	    CLOSE_RANGE_CLOEXEC));
	for (i = 1; i < 9; i++) {
		require_fd_flag(start + i, "CLOSE_RANGE_CLOEXEC",
		    "close-on-exec", FD_CLOEXEC, i >= 2 && i <= 5);
	}
	ATF_REQUIRE_INTEQ(0, close_range(start + 1, start + 8, 0));
}

ATF_TC_WITHOUT_HEAD(closerange_CLOFORK);
ATF_TC_BODY(closerange_CLOFORK, tc)
{
	int i, start;

	start = highest_fd();
	ATF_REQUIRE(start >= 2);

	for (i = 0; i < 8; i++)
		(void)devnull();
	ATF_REQUIRE_INTEQ(start + 8, highest_fd());

	ATF_REQUIRE_INTEQ(0, close_range(start + 2, start + 5,
	    CLOSE_RANGE_CLOFORK));
	for (i = 1; i < 9; i++) {
		require_fd_flag(start + i, "CLOSE_RANGE_CLOFORK",
		    "close-on-fork", FD_CLOFORK, i >= 2 && i <= 5);
	}
	ATF_REQUIRE_INTEQ(0, close_range(start + 1, start + 8, 0));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, closefrom_simple);
	ATF_TP_ADD_TC(tp, closefrom_with_holes);
	ATF_TP_ADD_TC(tp, closefrom_zero);
	ATF_TP_ADD_TC(tp, closefrom_negative_one);
	ATF_TP_ADD_TC(tp, closefrom_in_holes);
	ATF_TP_ADD_TC(tp, closerange_basic);
	ATF_TP_ADD_TC(tp, closefrom_zero_twice);
	ATF_TP_ADD_TC(tp, closerange_CLOEXEC);
	ATF_TP_ADD_TC(tp, closerange_CLOFORK);

	return (atf_no_error());
}
