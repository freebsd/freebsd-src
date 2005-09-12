/*-
 * Copyright (c) 2005 Robert N. M. Watson
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Regression test for piddling details of fifos.
 */

/*
 * All activity occurs within a temporary directory created early in the
 * test.
 */
char	temp_dir[PATH_MAX];

static void __unused
atexit_temp_dir(void)
{

	rmdir(temp_dir);
}

static void
makefifo(const char *fifoname, const char *testname)
{

	if (mkfifo(fifoname, 0700) < 0)
		err(-1, "%s: makefifo: mkfifo: %s", testname, fifoname);
}

static void
cleanfifo(const char *fifoname, int fd1, int fd2)
{

	if (fd1 != -1)
		close(fd1);
	if (fd2 != -1)
		close(fd2);
	(void)unlink(fifoname);
}

static int
openfifo(const char *fifoname, const char *testname, int *reader_fdp,
    int *writer_fdp)
{
	int error, fd1, fd2;

	fd1 = open(fifoname, O_RDONLY | O_NONBLOCK);
	if (fd1 < 0)
		return (-1);
	fd2 = open(fifoname, O_WRONLY | O_NONBLOCK);
	if (fd2 < 0) {
		error = errno;
		close(fd1);
		errno = error;
		return (-1);
	}
	*reader_fdp = fd1;
	*writer_fdp = fd2;

	return (0);
}

/*
 * POSIX does not allow lseek(2) on fifos, so we expect ESPIPE as a result.
 */
static void
test_lseek(void)
{
	int reader_fd, writer_fd;

	makefifo("testfifo", __func__);

	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd) < 0) {
		warn("%s: openfifo", __func__);
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	if (lseek(reader_fd, SEEK_CUR, 1) >= 0) {
		warnx("%s: lseek succeeded instead of returning ESPIPE",
		    __func__);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (errno != ESPIPE) {
		warn("%s: lseek returned instead of ESPIPE", __func__);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", reader_fd, writer_fd);
}

struct filter_entry {
	int		 fe_filter;
	const char	*fe_name;
	int		 fe_error;
	const char	*fe_errorname;
};

static const struct filter_entry good_filter_types[] = {
	{ EVFILT_READ, "EVFILT_READ", 0, "0" },
	{ EVFILT_WRITE, "EVFILT_WRITE", 0, "0" },
#if WORKING_EVFILT_VNODE_ON_FIFOS
	{ EVFILT_VNODE, "EVFILT_VNODE", EINVAL, "EINVAL" },
#endif
};
static const int good_filter_types_len = sizeof(good_filter_types) /
    sizeof(good_filter_types[0]);

static const struct filter_entry bad_filter_types[] = {
	{ EVFILT_NETDEV, "EVFILT_NETDEV", EINVAL, "EINVAL" },
};
static const int bad_filter_types_len = sizeof(bad_filter_types) /
    sizeof(bad_filter_types[0]);

/*
 * kqueue event-related tests are in fifo_io.c; however, that tests only
 * valid invocations of kqueue.  Check to make sure that some invalid filters
 * that are generally allowed on file descriptors are not allowed to be
 * registered with kqueue, and that if attempts are made, we get the right
 * error.
 */
static void
test_kqueue(void)
{
	int kqueue_fd, reader_fd, writer_fd;
	struct kevent kev_set;
	struct timespec timeout;
	int i, ret;

	makefifo("testfifo", __func__);

	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd) < 0) {
		warn("%s: openfifo", __func__);
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	kqueue_fd = kqueue();
	if (kqueue_fd < 0) {
		warn("%s: kqueue", __func__);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;

	for (i = 0; i < good_filter_types_len; i++) {
		bzero(&kev_set, sizeof(kev_set));
		EV_SET(&kev_set, reader_fd, good_filter_types[i].fe_filter,
		    EV_ADD, 0, 0, 0);
		ret = kevent(kqueue_fd, &kev_set, 1, NULL, 0, &timeout);
		if (ret < 0) {
			warn("%s: kevent: adding good filter %s", __func__,
			    good_filter_types[i].fe_name);
			close(kqueue_fd);
			cleanfifo("testfifo", reader_fd, writer_fd);
			exit(-1);
		}
		bzero(&kev_set, sizeof(kev_set));
		EV_SET(&kev_set, reader_fd, good_filter_types[i].fe_filter,
		    EV_DELETE, 0, 0, 0);
		ret = kevent(kqueue_fd, &kev_set, 1, NULL, 0, &timeout);
		if (ret < 0) {
			warn("%s: kevent: deleting good filter %s", __func__,
			    good_filter_types[i].fe_name);
			close(kqueue_fd);
			cleanfifo("testfifo", reader_fd, writer_fd);
			exit(-1);
		}
	}

	for (i = 0; i < bad_filter_types_len; i++) {
		bzero(&kev_set, sizeof(kev_set));
		printf("Trying %s\n", bad_filter_types[i].fe_name);
		EV_SET(&kev_set, reader_fd, bad_filter_types[i].fe_filter,
		    EV_ADD, 0, 0, 0);
		ret = kevent(kqueue_fd, &kev_set, 1, NULL, 0, &timeout);
		if (ret >= 0) {
			warnx("%s: kevent: bad filter %s succeeded, expected "
			    "EINVAL", __func__, bad_filter_types[i].fe_name);
			close(kqueue_fd);
			cleanfifo("testfifo", reader_fd, writer_fd);
			exit(-1);
		}
		if (errno != bad_filter_types[i].fe_error) {
			warn("%s: kevent: bad filter %s failed with error "
			    "not %s", __func__,
			    bad_filter_types[i].fe_name,
			    bad_filter_types[i].fe_errorname);
			close(kqueue_fd);
			cleanfifo("testfifo", reader_fd, writer_fd);
			exit(-1);
		}
	}

	close(kqueue_fd);
	cleanfifo("testfifo", reader_fd, writer_fd);
}

int
main(int argc, char *argv[])
{

	strcpy(temp_dir, "/tmp/fifo_misc.XXXXXXXXXXX");
	if (mkdtemp(temp_dir) == NULL)
		err(-1, "mkdtemp");
	atexit(atexit_temp_dir);

	if (chdir(temp_dir) < 0)
		err(-1, "chdir %s", temp_dir);

	test_lseek();
	test_kqueue();

	return (0);
}
