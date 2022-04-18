/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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
 * Basic regression tests for handling of O_PATH descriptors.
 */

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sys/memrange.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <aio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

#define	FMT_ERR(s)		s ": %s", strerror(errno)

#define	CHECKED_CLOSE(fd)	\
	ATF_REQUIRE_MSG(close(fd) == 0, FMT_ERR("close"))

/* Create a temporary regular file containing some data. */
static void
mktfile(char path[PATH_MAX], const char *template)
{
	char buf[BUFSIZ];
	int fd;

	snprintf(path, PATH_MAX, "%s", template);
	fd = mkstemp(path);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("mkstemp"));
	memset(buf, 0, sizeof(buf));
	ATF_REQUIRE_MSG(write(fd, buf, sizeof(buf)) == sizeof(buf),
	    FMT_ERR("write"));
	CHECKED_CLOSE(fd);
}

/* Make a temporary directory. */
static void
mktdir(char path[PATH_MAX], const char *template)
{
	snprintf(path, PATH_MAX, "%s", template);
	ATF_REQUIRE_MSG(mkdtemp(path) == path, FMT_ERR("mkdtemp"));
}

/* Wait for a child process to exit with status 0. */
static void
waitchild(pid_t child, int exstatus)
{
	int error, status;

	error = waitpid(child, &status, 0);
	ATF_REQUIRE_MSG(error != -1, FMT_ERR("waitpid"));
	ATF_REQUIRE_MSG(WIFEXITED(status), "child exited abnormally, status %d",
	    status);
	ATF_REQUIRE_MSG(WEXITSTATUS(status) == exstatus,
	    "child exit status is %d, expected %d",
	    WEXITSTATUS(status), exstatus);
}

ATF_TC_WITHOUT_HEAD(path_access);
ATF_TC_BODY(path_access, tc)
{
	char path[PATH_MAX];
	struct stat sb;
	struct timespec ts[2];
	struct timeval tv[2];
	int pathfd;

	mktfile(path, "path_access.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	ATF_REQUIRE_ERRNO(EBADF, fchmod(pathfd, 0666) == -1);
	ATF_REQUIRE_ERRNO(EBADF, fchown(pathfd, getuid(), getgid()) == -1);
	ATF_REQUIRE_ERRNO(EBADF, fchflags(pathfd, UF_NODUMP) == -1);
	memset(tv, 0, sizeof(tv));
	ATF_REQUIRE_ERRNO(EBADF, futimes(pathfd, tv) == -1);
	memset(ts, 0, sizeof(ts));
	ATF_REQUIRE_ERRNO(EBADF, futimens(pathfd, ts) == -1);

	/* fpathconf(2) and fstat(2) are permitted. */
	ATF_REQUIRE_MSG(fstat(pathfd, &sb) == 0, FMT_ERR("fstat"));
	ATF_REQUIRE_MSG(fpathconf(pathfd, _PC_LINK_MAX) != -1,
	    FMT_ERR("fpathconf"));

	CHECKED_CLOSE(pathfd);
}

/* Basic tests to verify that AIO operations fail. */
ATF_TC_WITHOUT_HEAD(path_aio);
ATF_TC_BODY(path_aio, tc)
{
	struct aiocb aio;
	char buf[BUFSIZ], path[PATH_MAX];
	int pathfd;

	mktfile(path, "path_aio.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	memset(&aio, 0, sizeof(aio));
	aio.aio_buf = buf;
	aio.aio_nbytes = sizeof(buf);
	aio.aio_fildes = pathfd;
	aio.aio_offset = 0;

	ATF_REQUIRE_ERRNO(EBADF, aio_read(&aio) == -1);
	ATF_REQUIRE_ERRNO(EBADF, aio_write(&aio) == -1);
	ATF_REQUIRE_ERRNO(EBADF, aio_fsync(O_SYNC, &aio) == -1);
	ATF_REQUIRE_ERRNO(EBADF, aio_fsync(O_DSYNC, &aio) == -1);

	CHECKED_CLOSE(pathfd);
}

/* Basic tests to verify that Capsicum restrictions apply to path fds. */
ATF_TC_WITHOUT_HEAD(path_capsicum);
ATF_TC_BODY(path_capsicum, tc)
{
	char path[PATH_MAX];
	cap_rights_t rights;
	int truefd;
	pid_t child;

	mktfile(path, "path_capsicum.XXXXXX");

	/* Make sure that filesystem namespace restrictions apply to O_PATH. */
	child = fork();
	ATF_REQUIRE_MSG(child != -1, FMT_ERR("fork"));
	if (child == 0) {
		if (cap_enter() != 0)
			_exit(1);
		if (open(path, O_PATH) >= 0)
			_exit(2);
		if (errno != ECAPMODE)
			_exit(3);
		if (open("/usr/bin/true", O_PATH | O_EXEC) >= 0)
			_exit(4);
		if (errno != ECAPMODE)
			_exit(5);
		_exit(0);
	}
	waitchild(child, 0);

	/* Make sure that CAP_FEXECVE is required. */
	child = fork();
	ATF_REQUIRE_MSG(child != -1, FMT_ERR("fork"));
	if (child == 0) {
		truefd = open("/usr/bin/true", O_PATH | O_EXEC);
		if (truefd < 0)
			_exit(1);
		cap_rights_init(&rights);
		if (cap_rights_limit(truefd, &rights) != 0)
			_exit(2);
		(void)fexecve(truefd,
		    (char * const[]){__DECONST(char *, "/usr/bin/true"), NULL},
		    NULL);
		if (errno != ENOTCAPABLE)
			_exit(3);
		_exit(4);
	}
	waitchild(child, 4);
}

/* Make sure that ptrace(PT_COREDUMP) cannot be used to write to a path fd. */
ATF_TC_WITHOUT_HEAD(path_coredump);
ATF_TC_BODY(path_coredump, tc)
{
	char path[PATH_MAX];
	struct ptrace_coredump pc;
	int error, pathfd, status;
	pid_t child;

	mktdir(path, "path_coredump.XXXXXX");

	child = fork();
	ATF_REQUIRE_MSG(child != -1, FMT_ERR("fork"));
	if (child == 0) {
		while (true)
			(void)sleep(1);
	}

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	error = ptrace(PT_ATTACH, child, 0, 0);
	ATF_REQUIRE_MSG(error == 0, FMT_ERR("ptrace"));
	error = waitpid(child, &status, 0);
	ATF_REQUIRE_MSG(error != -1, FMT_ERR("waitpid"));
	ATF_REQUIRE_MSG(WIFSTOPPED(status), "unexpected status %d", status);

	pc.pc_fd = pathfd;
	pc.pc_flags = 0;
	pc.pc_limit = 0;
	error = ptrace(PT_COREDUMP, child, (void *)&pc, sizeof(pc));
	ATF_REQUIRE_ERRNO(EBADF, error == -1);

	error = ptrace(PT_DETACH, child, 0, 0);
	ATF_REQUIRE_MSG(error == 0, FMT_ERR("ptrace"));

	ATF_REQUIRE_MSG(kill(child, SIGKILL) == 0, FMT_ERR("kill"));

	CHECKED_CLOSE(pathfd);
}

/* Verify operations on directory path descriptors. */
ATF_TC_WITHOUT_HEAD(path_directory);
ATF_TC_BODY(path_directory, tc)
{
	struct dirent de;
	struct stat sb;
	char path[PATH_MAX];
	int fd, pathfd;

	mktdir(path, "path_directory.XXXXXX");

	pathfd = open(path, O_PATH | O_DIRECTORY);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	/* Should not be possible to list directory entries. */
	ATF_REQUIRE_ERRNO(EBADF,
	    getdirentries(pathfd, (char *)&de, sizeof(de), NULL) == -1);

	/* It should be possible to create files under pathfd. */
	fd = openat(pathfd, "test", O_RDWR | O_CREAT, 0600);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("open"));
	ATF_REQUIRE_MSG(fstatat(pathfd, "test", &sb, 0) == 0,
	    FMT_ERR("fstatat"));
	CHECKED_CLOSE(fd);

	/* ... but doing so requires write access. */
	if (geteuid() != 0) {
		ATF_REQUIRE_ERRNO(EBADF, fchmod(pathfd, 0500) == -1);
		ATF_REQUIRE_MSG(chmod(path, 0500) == 0, FMT_ERR("chmod"));
		ATF_REQUIRE_ERRNO(EACCES,
		    openat(pathfd, "test2", O_RDWR | O_CREAT, 0600) < 0);
	}

	/* fchdir(2) is permitted. */
	ATF_REQUIRE_MSG(fchdir(pathfd) == 0, FMT_ERR("fchdir"));

	CHECKED_CLOSE(pathfd);
}

/* Verify access permission checking for a directory path fd. */
ATF_TC_WITH_CLEANUP(path_directory_not_root);
ATF_TC_HEAD(path_directory_not_root, tc)
{
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(path_directory_not_root, tc)
{
	char path[PATH_MAX];
	int pathfd;

	mktdir(path, "path_directory.XXXXXX");

	pathfd = open(path, O_PATH | O_DIRECTORY);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	ATF_REQUIRE_ERRNO(EBADF, fchmod(pathfd, 0500) == -1);
	ATF_REQUIRE_MSG(chmod(path, 0500) == 0, FMT_ERR("chmod"));
	ATF_REQUIRE_ERRNO(EACCES,
	    openat(pathfd, "test2", O_RDWR | O_CREAT, 0600) < 0);

	CHECKED_CLOSE(pathfd);
}
ATF_TC_CLEANUP(path_directory_not_root, tc)
{
}

/* Validate system calls that handle AT_EMPTY_PATH. */
ATF_TC_WITHOUT_HEAD(path_empty);
ATF_TC_BODY(path_empty, tc)
{
	char path[PATH_MAX];
	struct timespec ts[2];
	struct stat sb;
	int pathfd;

	mktfile(path, "path_empty.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	/* Various *at operations should work on path fds. */
	ATF_REQUIRE_MSG(faccessat(pathfd, "", F_OK, AT_EMPTY_PATH) == 0,
	    FMT_ERR("faccessat"));
	ATF_REQUIRE_MSG(chflagsat(pathfd, "", UF_NODUMP, AT_EMPTY_PATH) == 0,
	    FMT_ERR("chflagsat"));
	ATF_REQUIRE_MSG(fchmodat(pathfd, "", 0600, AT_EMPTY_PATH) == 0,
	    FMT_ERR("fchmodat"));
	ATF_REQUIRE_MSG(fchownat(pathfd, "", getuid(), getgid(),
	    AT_EMPTY_PATH) == 0, FMT_ERR("fchownat"));
	ATF_REQUIRE_MSG(fstatat(pathfd, "", &sb, AT_EMPTY_PATH) == 0,
	    FMT_ERR("fstatat"));
	ATF_REQUIRE_MSG(sb.st_size == BUFSIZ,
	    "unexpected size %ju", (uintmax_t)sb.st_size);
	memset(ts, 0, sizeof(ts));
	ATF_REQUIRE_MSG(utimensat(pathfd, "", ts, AT_EMPTY_PATH) == 0,
	    FMT_ERR("utimensat"));

	CHECKED_CLOSE(pathfd);
}

/* Verify that various operations on a path fd have access checks. */
ATF_TC_WITH_CLEANUP(path_empty_not_root);
ATF_TC_HEAD(path_empty_not_root, tc)
{
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(path_empty_not_root, tc)
{
	int pathfd;

	pathfd = open("/dev/null", O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	ATF_REQUIRE_ERRNO(EPERM,
	    chflagsat(pathfd, "", UF_NODUMP, AT_EMPTY_PATH) == -1);
	ATF_REQUIRE_ERRNO(EPERM,
	    fchownat(pathfd, "", getuid(), getgid(), AT_EMPTY_PATH) == -1);
	ATF_REQUIRE_ERRNO(EPERM,
	    fchmodat(pathfd, "", 0600, AT_EMPTY_PATH) == -1);
	ATF_REQUIRE_ERRNO(EPERM,
	    linkat(pathfd, "", AT_FDCWD, "test", AT_EMPTY_PATH) == -1);

	CHECKED_CLOSE(pathfd);
}
ATF_TC_CLEANUP(path_empty_not_root, tc)
{
}

/* Test linkat(2) with AT_EMPTY_PATH, which requires privileges. */
ATF_TC_WITH_CLEANUP(path_empty_root);
ATF_TC_HEAD(path_empty_root, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(path_empty_root, tc)
{
	char path[PATH_MAX];
	struct stat sb, sb2;
	int pathfd;

	mktfile(path, "path_empty_root.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));
	ATF_REQUIRE_MSG(fstatat(pathfd, "", &sb, AT_EMPTY_PATH) == 0,
	    FMT_ERR("fstatat"));

	ATF_REQUIRE_MSG(linkat(pathfd, "", AT_FDCWD, "test", AT_EMPTY_PATH) ==
	    0, FMT_ERR("linkat"));
	ATF_REQUIRE_MSG(fstatat(AT_FDCWD, "test", &sb2, 0) == 0,
	    FMT_ERR("fstatat"));
	ATF_REQUIRE_MSG(sb.st_dev == sb2.st_dev, "st_dev mismatch");
	ATF_REQUIRE_MSG(sb.st_ino == sb2.st_ino, "st_ino mismatch");

	CHECKED_CLOSE(pathfd);

}
ATF_TC_CLEANUP(path_empty_root, tc)
{
}

/* poll(2) never returns an event for path fds, but kevent(2) does. */
ATF_TC_WITHOUT_HEAD(path_event);
ATF_TC_BODY(path_event, tc)
{
	char buf[BUFSIZ], path[PATH_MAX];
	struct kevent ev;
	struct pollfd pollfd;
	int kq, pathfd;

	mktfile(path, "path_event.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	/* poll(2) should return POLLNVAL. */
	pollfd.fd = pathfd;
	pollfd.events = POLLIN;
	pollfd.revents = 0;
	ATF_REQUIRE_MSG(poll(&pollfd, 1, 0) == 1, FMT_ERR("poll"));
	ATF_REQUIRE_MSG(pollfd.revents == POLLNVAL, "unexpected revents %x",
	    pollfd.revents);
	pollfd.events = POLLOUT;
	pollfd.revents = 0;
	ATF_REQUIRE_MSG(poll(&pollfd, 1, 0) == 1, FMT_ERR("poll"));
	ATF_REQUIRE_MSG(pollfd.revents == POLLNVAL, "unexpected revents %x",
	    pollfd.revents);

	/* Try to get a EVFILT_READ event through a path fd. */
	kq = kqueue();
	ATF_REQUIRE_MSG(kq >= 0, FMT_ERR("kqueue"));
	EV_SET(&ev, pathfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
	ATF_REQUIRE_MSG(kevent(kq, &ev, 1, NULL, 0, NULL) == 0,
	    FMT_ERR("kevent"));
	ATF_REQUIRE_MSG(kevent(kq, NULL, 0, &ev, 1, NULL) == 1,
	    FMT_ERR("kevent"));
	ATF_REQUIRE_MSG((ev.flags & EV_ERROR) == 0, "EV_ERROR is set");
	ATF_REQUIRE_MSG(ev.data == sizeof(buf),
	    "data is %jd", (intmax_t)ev.data);
	EV_SET(&ev, pathfd, EVFILT_READ, EV_DELETE, 0, 0, 0);
	ATF_REQUIRE_MSG(kevent(kq, &ev, 1, NULL, 0, NULL) == 0,
	    FMT_ERR("kevent"));

	/* Try to get a EVFILT_VNODE/NOTE_DELETE event through a path fd. */
	EV_SET(&ev, pathfd, EVFILT_VNODE, EV_ADD | EV_ENABLE, NOTE_DELETE, 0,
	    0);
	ATF_REQUIRE_MSG(kevent(kq, &ev, 1, NULL, 0, NULL) == 0,
	    FMT_ERR("kevent"));
	ATF_REQUIRE_MSG(funlinkat(AT_FDCWD, path, pathfd, 0) == 0,
	    FMT_ERR("funlinkat"));
	ATF_REQUIRE_MSG(kevent(kq, NULL, 0, &ev, 1, NULL) == 1,
	    FMT_ERR("kevent"));
	ATF_REQUIRE_MSG(ev.fflags == NOTE_DELETE,
	    "unexpected fflags %#x", ev.fflags);
	EV_SET(&ev, pathfd, EVFILT_VNODE, EV_DELETE, 0, 0, 0);
	ATF_REQUIRE_MSG(kevent(kq, &ev, 1, NULL, 0, NULL) == 0,
	    FMT_ERR("kevent"));

	CHECKED_CLOSE(kq);
	CHECKED_CLOSE(pathfd);
}

/* Check various fcntl(2) operations on a path desriptor. */
ATF_TC_WITHOUT_HEAD(path_fcntl);
ATF_TC_BODY(path_fcntl, tc)
{
	char path[PATH_MAX];
	int flags, pathfd, pathfd2;

	mktfile(path, "path_fcntl.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	/* O_PATH should appear in the fd flags. */
	flags = fcntl(pathfd, F_GETFL);
	ATF_REQUIRE_MSG(flags != -1, FMT_ERR("fcntl"));
	ATF_REQUIRE_MSG((flags & O_PATH) != 0, "O_PATH not set");

	ATF_REQUIRE_ERRNO(EBADF,
	    fcntl(pathfd, F_SETFL, flags & ~O_PATH));
	ATF_REQUIRE_ERRNO(EBADF,
	    fcntl(pathfd, F_SETFL, flags | O_APPEND));

	/* A dup'ed O_PATH fd had better have O_PATH set too. */
	pathfd2 = fcntl(pathfd, F_DUPFD, 0);
	ATF_REQUIRE_MSG(pathfd2 >= 0, FMT_ERR("fcntl"));
	flags = fcntl(pathfd2, F_GETFL);
	ATF_REQUIRE_MSG(flags != -1, FMT_ERR("fcntl"));
	ATF_REQUIRE_MSG((flags & O_PATH) != 0, "O_PATH not set");
	CHECKED_CLOSE(pathfd2);

	/* Double check with dup(2). */
	pathfd2 = dup(pathfd);
	ATF_REQUIRE_MSG(pathfd2 >= 0, FMT_ERR("dup"));
	flags = fcntl(pathfd2, F_GETFL);
	ATF_REQUIRE_MSG(flags != -1, FMT_ERR("fcntl"));
	ATF_REQUIRE_MSG((flags & O_PATH) != 0, "O_PATH not set");
	CHECKED_CLOSE(pathfd2);

	/* It should be possible to set O_CLOEXEC. */
	ATF_REQUIRE_MSG(fcntl(pathfd, F_SETFD, FD_CLOEXEC) == 0,
	    FMT_ERR("fcntl"));
	ATF_REQUIRE_MSG(fcntl(pathfd, F_GETFD) == FD_CLOEXEC,
	    FMT_ERR("fcntl"));

	CHECKED_CLOSE(pathfd);
}

/* Verify that we can execute a file opened with O_PATH. */
ATF_TC_WITHOUT_HEAD(path_fexecve);
ATF_TC_BODY(path_fexecve, tc)
{
	char path[PATH_MAX];
	pid_t child;
	int fd, pathfd;

	child = fork();
	ATF_REQUIRE_MSG(child != -1, FMT_ERR("fork"));
	if (child == 0) {
		pathfd = open("/usr/bin/true", O_PATH | O_EXEC);
		if (pathfd < 0)
			_exit(1);
		fexecve(pathfd,
		    (char * const[]){__DECONST(char *, "/usr/bin/true"), NULL},
		    NULL);
		_exit(2);
	}
	waitchild(child, 0);

	/*
	 * Also verify that access permissions are checked when opening with
	 * O_PATH.
	 */
	snprintf(path, sizeof(path), "path_fexecve.XXXXXX");
	ATF_REQUIRE_MSG(mktemp(path) == path, FMT_ERR("mktemp"));

	fd = open(path, O_CREAT | O_RDONLY, 0600);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("open"));

	pathfd = open(path, O_PATH | O_EXEC);
	ATF_REQUIRE_ERRNO(EACCES, pathfd < 0);
}

/* Make sure that O_PATH restrictions apply to named pipes as well. */
ATF_TC_WITHOUT_HEAD(path_fifo);
ATF_TC_BODY(path_fifo, tc)
{
	char path[PATH_MAX], buf[BUFSIZ];
	struct kevent ev;
	int kq, pathfd;

	snprintf(path, sizeof(path), "path_fifo.XXXXXX");
	ATF_REQUIRE_MSG(mktemp(path) == path, FMT_ERR("mktemp"));

	ATF_REQUIRE_MSG(mkfifo(path, 0666) == 0, FMT_ERR("mkfifo"));

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));
	memset(buf, 0, sizeof(buf));
	ATF_REQUIRE_ERRNO(EBADF, write(pathfd, buf, sizeof(buf)));
	ATF_REQUIRE_ERRNO(EBADF, read(pathfd, buf, sizeof(buf)));

	kq = kqueue();
	ATF_REQUIRE_MSG(kq >= 0, FMT_ERR("kqueue"));
	EV_SET(&ev, pathfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
	ATF_REQUIRE_ERRNO(EBADF, kevent(kq, &ev, 1, NULL, 0, NULL) == -1);

	CHECKED_CLOSE(pathfd);
}

/* Files may be unlinked using a path fd. */
ATF_TC_WITHOUT_HEAD(path_funlinkat);
ATF_TC_BODY(path_funlinkat, tc)
{
	char path[PATH_MAX];
	struct stat sb;
	int pathfd;

	mktfile(path, "path_rights.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	ATF_REQUIRE_MSG(funlinkat(AT_FDCWD, path, pathfd, 0) == 0,
	    FMT_ERR("funlinkat"));
	ATF_REQUIRE_ERRNO(ENOENT, stat(path, &sb) == -1);

	CHECKED_CLOSE(pathfd);
}

/* Verify that various I/O operations fail on an O_PATH descriptor. */
ATF_TC_WITHOUT_HEAD(path_io);
ATF_TC_BODY(path_io, tc)
{
	char path[PATH_MAX], path2[PATH_MAX];
	char buf[BUFSIZ];
	struct iovec iov;
	size_t page_size;
	int error, fd, pathfd, sd[2];

	/* It shouldn't be possible to create new files with O_PATH. */
	snprintf(path, sizeof(path), "path_io.XXXXXX");
	ATF_REQUIRE_MSG(mktemp(path) == path, FMT_ERR("mktemp"));
	ATF_REQUIRE_ERRNO(ENOENT, open(path, O_PATH | O_CREAT, 0600) < 0);

	/* Create a non-empty file for use in the rest of the tests. */
	mktfile(path, "path_io.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	/* Make sure that basic I/O operations aren't possible. */
	iov.iov_base = path;
	iov.iov_len = strlen(path);
	ATF_REQUIRE_ERRNO(EBADF,
	    write(pathfd, iov.iov_base, iov.iov_len) == -1);
	ATF_REQUIRE_ERRNO(EBADF,
	    pwrite(pathfd, iov.iov_base, iov.iov_len, 0) == -1);
	ATF_REQUIRE_ERRNO(EBADF,
	    writev(pathfd, &iov, 1) == -1);
	ATF_REQUIRE_ERRNO(EBADF,
	    pwritev(pathfd, &iov, 1, 0) == -1);
	ATF_REQUIRE_ERRNO(EBADF,
	    read(pathfd, path, 1) == -1);
	ATF_REQUIRE_ERRNO(EBADF,
	    pread(pathfd, path, 1, 0) == -1);
	ATF_REQUIRE_ERRNO(EBADF,
	    readv(pathfd, &iov, 1) == -1);
	ATF_REQUIRE_ERRNO(EBADF,
	    preadv(pathfd, &iov, 1, 0) == -1);

	/* copy_file_range() should not be permitted. */
	mktfile(path2, "path_io.XXXXXX");
	fd = open(path2, O_RDWR);
	ATF_REQUIRE_ERRNO(EBADF,
	    copy_file_range(fd, NULL, pathfd, NULL, sizeof(buf), 0) == -1);
	ATF_REQUIRE_ERRNO(EBADF,
	    copy_file_range(pathfd, NULL, fd, NULL, sizeof(buf), 0) == -1);
	CHECKED_CLOSE(fd);

	/* sendfile() should not be permitted. */
	ATF_REQUIRE_MSG(socketpair(PF_LOCAL, SOCK_STREAM, 0, sd) == 0,
	    FMT_ERR("socketpair"));
	ATF_REQUIRE_ERRNO(EBADF,
	    sendfile(pathfd, sd[0], 0, 0, NULL, NULL, 0));
	CHECKED_CLOSE(sd[0]);
	CHECKED_CLOSE(sd[1]);

	/* No seeking. */
	ATF_REQUIRE_ERRNO(ESPIPE,
	    lseek(pathfd, 0, SEEK_SET) == -1);

	/* No operations on the file extent. */
	ATF_REQUIRE_ERRNO(EINVAL,
	    ftruncate(pathfd, 0) == -1);
	error = posix_fallocate(pathfd, 0, sizeof(buf) * 2);
	ATF_REQUIRE_MSG(error == ESPIPE, "posix_fallocate() returned %d", error);
	error = posix_fadvise(pathfd, 0, sizeof(buf), POSIX_FADV_NORMAL);
	ATF_REQUIRE_MSG(error == ESPIPE, "posix_fadvise() returned %d", error);

	/* mmap() is not allowed. */
	page_size = getpagesize();
	ATF_REQUIRE_ERRNO(ENODEV,
	    mmap(NULL, page_size, PROT_READ, MAP_SHARED, pathfd, 0) ==
	    MAP_FAILED);
	ATF_REQUIRE_ERRNO(ENODEV,
	    mmap(NULL, page_size, PROT_NONE, MAP_SHARED, pathfd, 0) ==
	    MAP_FAILED);
	ATF_REQUIRE_ERRNO(ENODEV,
	    mmap(NULL, page_size, PROT_READ, MAP_PRIVATE, pathfd, 0) ==
	    MAP_FAILED);

	/* No fsync() or fdatasync(). */
	ATF_REQUIRE_ERRNO(EBADF, fsync(pathfd) == -1);
	ATF_REQUIRE_ERRNO(EBADF, fdatasync(pathfd) == -1);

	CHECKED_CLOSE(pathfd);
}

/* ioctl(2) is not permitted on path fds. */
ATF_TC_WITHOUT_HEAD(path_ioctl);
ATF_TC_BODY(path_ioctl, tc)
{
	char path[PATH_MAX];
	struct mem_extract me;
	int pathfd, val;

	mktfile(path, "path_ioctl.XXXXXX");

	/* Standard file descriptor ioctls should fail. */
	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	val = 0;
	ATF_REQUIRE_ERRNO(EBADF, ioctl(pathfd, FIONBIO, &val) == -1);
	ATF_REQUIRE_ERRNO(EBADF, ioctl(pathfd, FIONREAD, &val) == -1);
	ATF_REQUIRE_ERRNO(EBADF, ioctl(pathfd, FIONWRITE, &val) == -1);
	ATF_REQUIRE_ERRNO(EBADF, ioctl(pathfd, FIONSPACE, &val) == -1);

	CHECKED_CLOSE(pathfd);

	/* Device ioctls should fail. */
	pathfd = open("/dev/mem", O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	me.me_vaddr = (uintptr_t)&me;
	ATF_REQUIRE_ERRNO(EBADF, ioctl(pathfd, MEM_EXTRACT_PADDR, &me) == -1);

	CHECKED_CLOSE(pathfd);
}

ATF_TC_WITHOUT_HEAD(path_lock);
ATF_TC_BODY(path_lock, tc)
{
	char buf[BUFSIZ], path[PATH_MAX];
	struct flock flk;
	int fd, pathfd;

	snprintf(path, sizeof(path), "path_rights.XXXXXX");
	fd = mkostemp(path, O_SHLOCK);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("mkostemp"));
	memset(buf, 0, sizeof(buf));
	ATF_REQUIRE_MSG(write(fd, buf, sizeof(buf)) == sizeof(buf),
	    FMT_ERR("write()"));

	/* Verify that O_EXLOCK is ignored when combined with O_PATH. */
	pathfd = open(path, O_PATH | O_EXLOCK);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	CHECKED_CLOSE(fd);

	/* flock(2) is prohibited. */
	ATF_REQUIRE_ERRNO(EOPNOTSUPP, flock(pathfd, LOCK_SH) == -1);
	ATF_REQUIRE_ERRNO(EOPNOTSUPP, flock(pathfd, LOCK_EX) == -1);
	ATF_REQUIRE_ERRNO(EOPNOTSUPP, flock(pathfd, LOCK_SH | LOCK_NB) == -1);
	ATF_REQUIRE_ERRNO(EOPNOTSUPP, flock(pathfd, LOCK_EX | LOCK_NB) == -1);
	ATF_REQUIRE_ERRNO(EOPNOTSUPP, flock(pathfd, LOCK_UN) == -1);

	/* fcntl(2) file locks are prohibited. */
	memset(&flk, 0, sizeof(flk));
	flk.l_whence = SEEK_CUR;
	ATF_REQUIRE_ERRNO(EBADF, fcntl(pathfd, F_GETLK, &flk) == -1);
	flk.l_type = F_RDLCK;
	ATF_REQUIRE_ERRNO(EBADF, fcntl(pathfd, F_SETLK, &flk) == -1);
	ATF_REQUIRE_ERRNO(EBADF, fcntl(pathfd, F_SETLKW, &flk) == -1);
	flk.l_type = F_WRLCK;
	ATF_REQUIRE_ERRNO(EBADF, fcntl(pathfd, F_SETLK, &flk) == -1);
	ATF_REQUIRE_ERRNO(EBADF, fcntl(pathfd, F_SETLKW, &flk) == -1);

	CHECKED_CLOSE(pathfd);
}

/*
 * Verify fstatat(AT_EMPTY_PATH) on non-regular dirfd.
 * Verify that fstatat(AT_EMPTY_PATH) on NULL path returns EFAULT.
 */
ATF_TC_WITHOUT_HEAD(path_pipe_fstatat);
ATF_TC_BODY(path_pipe_fstatat, tc)
{
	struct stat sb;
	int fd[2];

	ATF_REQUIRE_MSG(pipe(fd) == 0, FMT_ERR("pipe"));
	ATF_REQUIRE_MSG(fstatat(fd[0], "", &sb, AT_EMPTY_PATH) == 0,
	    FMT_ERR("fstatat pipe"));
	ATF_REQUIRE_ERRNO(EFAULT, fstatat(fd[0], NULL, &sb,
	    AT_EMPTY_PATH) == -1);
	CHECKED_CLOSE(fd[0]);
	CHECKED_CLOSE(fd[1]);
}

/* Verify that we can send an O_PATH descriptor over a unix socket. */
ATF_TC_WITHOUT_HEAD(path_rights);
ATF_TC_BODY(path_rights, tc)
{
	char path[PATH_MAX];
	struct cmsghdr *cmsg;
	struct msghdr msg;
	struct iovec iov;
	int flags, pathfd, pathfd_copy, sd[2];
	char c;

	ATF_REQUIRE_MSG(socketpair(PF_LOCAL, SOCK_STREAM, 0, sd) == 0,
	    FMT_ERR("socketpair"));

	mktfile(path, "path_rights.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	/* Package up the O_PATH and send it over the socket pair. */
	cmsg = malloc(CMSG_SPACE(sizeof(pathfd)));
	ATF_REQUIRE_MSG(cmsg != NULL, FMT_ERR("malloc"));

	cmsg->cmsg_len = CMSG_LEN(sizeof(pathfd));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	*(int *)(void *)CMSG_DATA(cmsg) = pathfd;

	c = 0;
	iov.iov_base = &c;
	iov.iov_len = 1;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsg;
	msg.msg_controllen = CMSG_SPACE(sizeof(pathfd));

	ATF_REQUIRE_MSG(sendmsg(sd[0], &msg, 0) == sizeof(c),
	    FMT_ERR("sendmsg"));

	/* Grab the pathfd copy from the other end of the pair. */
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsg;
	msg.msg_controllen = CMSG_SPACE(sizeof(pathfd));

	ATF_REQUIRE_MSG(recvmsg(sd[1], &msg, 0) == 1,
	    FMT_ERR("recvmsg"));
	pathfd_copy = *(int *)(void *)CMSG_DATA(cmsg);
	ATF_REQUIRE_MSG(pathfd_copy != pathfd,
	    "pathfd and pathfd_copy are equal");

	/* Verify that the copy has O_PATH properties. */
	flags = fcntl(pathfd_copy, F_GETFL);
	ATF_REQUIRE_MSG(flags != -1, FMT_ERR("fcntl"));
	ATF_REQUIRE_MSG((flags & O_PATH) != 0, "O_PATH is not set");
	ATF_REQUIRE_ERRNO(EBADF,
	    read(pathfd_copy, &c, 1) == -1);
	ATF_REQUIRE_ERRNO(EBADF,
	    write(pathfd_copy, &c, 1) == -1);

	CHECKED_CLOSE(pathfd);
	CHECKED_CLOSE(pathfd_copy);
	CHECKED_CLOSE(sd[0]);
	CHECKED_CLOSE(sd[1]);
}

/* Verify that a local socket can be opened with O_PATH. */
ATF_TC_WITHOUT_HEAD(path_unix);
ATF_TC_BODY(path_unix, tc)
{
	char buf[BUFSIZ], path[PATH_MAX];
	struct kevent ev;
	struct sockaddr_un sun;
	struct stat sb;
	int kq, pathfd, sd;

	snprintf(path, sizeof(path), "path_unix.XXXXXX");
	ATF_REQUIRE_MSG(mktemp(path) == path, FMT_ERR("mktemp"));

	sd = socket(PF_LOCAL, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(sd >= 0, FMT_ERR("socket"));

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = PF_LOCAL;
	(void)strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	ATF_REQUIRE_MSG(bind(sd, (struct sockaddr *)&sun, SUN_LEN(&sun)) == 0,
	    FMT_ERR("bind"));

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	ATF_REQUIRE_MSG(fstatat(pathfd, "", &sb, AT_EMPTY_PATH) == 0,
	    FMT_ERR("fstatat"));
	ATF_REQUIRE_MSG(sb.st_mode & S_IFSOCK, "socket mode %#x", sb.st_mode);
	ATF_REQUIRE_MSG(sb.st_ino != 0, "socket has inode number 0");

	memset(buf, 0, sizeof(buf));
	ATF_REQUIRE_ERRNO(EBADF, write(pathfd, buf, sizeof(buf)));
	ATF_REQUIRE_ERRNO(EBADF, read(pathfd, buf, sizeof(buf)));

	/* kevent() is disallowed with sockets. */
	kq = kqueue();
	ATF_REQUIRE_MSG(kq >= 0, FMT_ERR("kqueue"));
	EV_SET(&ev, pathfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
	ATF_REQUIRE_ERRNO(EBADF, kevent(kq, &ev, 1, NULL, 0, NULL) == -1);

	/* Should not be able to open a socket without O_PATH. */
	ATF_REQUIRE_ERRNO(EOPNOTSUPP, openat(pathfd, "", O_EMPTY_PATH) == -1);

	ATF_REQUIRE_MSG(funlinkat(AT_FDCWD, path, pathfd, 0) == 0,
	    FMT_ERR("funlinkat"));

	CHECKED_CLOSE(sd);
	CHECKED_CLOSE(pathfd);
}

/*
 * Check that we can perform operations using an O_PATH fd for an unlinked file.
 */
ATF_TC_WITHOUT_HEAD(path_unlinked);
ATF_TC_BODY(path_unlinked, tc)
{
	char path[PATH_MAX];
	struct stat sb;
	int pathfd;

	mktfile(path, "path_rights.XXXXXX");

	pathfd = open(path, O_PATH);
	ATF_REQUIRE_MSG(pathfd >= 0, FMT_ERR("open"));

	ATF_REQUIRE_MSG(fstatat(pathfd, "", &sb, AT_EMPTY_PATH) == 0,
	    FMT_ERR("fstatat"));
	ATF_REQUIRE(sb.st_nlink == 1);
	ATF_REQUIRE_MSG(fstat(pathfd, &sb) == 0, FMT_ERR("fstat"));
	ATF_REQUIRE(sb.st_nlink == 1);

	ATF_REQUIRE_MSG(unlink(path) == 0, FMT_ERR("unlink"));

	ATF_REQUIRE_MSG(fstatat(pathfd, "", &sb, AT_EMPTY_PATH) == 0,
	    FMT_ERR("fstatat"));
	ATF_REQUIRE(sb.st_nlink == 0);
	ATF_REQUIRE_MSG(fstat(pathfd, &sb) == 0, FMT_ERR("fstat"));
	ATF_REQUIRE(sb.st_nlink == 0);

	CHECKED_CLOSE(pathfd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, path_access);
	ATF_TP_ADD_TC(tp, path_aio);
	ATF_TP_ADD_TC(tp, path_capsicum);
	ATF_TP_ADD_TC(tp, path_coredump);
	ATF_TP_ADD_TC(tp, path_directory);
	ATF_TP_ADD_TC(tp, path_directory_not_root);
	ATF_TP_ADD_TC(tp, path_empty);
	ATF_TP_ADD_TC(tp, path_empty_not_root);
	ATF_TP_ADD_TC(tp, path_empty_root);
	ATF_TP_ADD_TC(tp, path_event);
	ATF_TP_ADD_TC(tp, path_fcntl);
	ATF_TP_ADD_TC(tp, path_fexecve);
	ATF_TP_ADD_TC(tp, path_fifo);
	ATF_TP_ADD_TC(tp, path_funlinkat);
	ATF_TP_ADD_TC(tp, path_io);
	ATF_TP_ADD_TC(tp, path_ioctl);
	ATF_TP_ADD_TC(tp, path_lock);
	ATF_TP_ADD_TC(tp, path_pipe_fstatat);
	ATF_TP_ADD_TC(tp, path_rights);
	ATF_TP_ADD_TC(tp, path_unix);
	ATF_TP_ADD_TC(tp, path_unlinked);

	return (atf_no_error());
}
