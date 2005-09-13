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
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Regression test to exercise POSIX fifo I/O.
 *
 * We test a number of aspect of behavior, including:
 *
 * - If there's no data to read, then for blocking fifos, we block, and for
 *   non-blocking, we return EAGAIN.
 *
 * - If we write ten bytes, ten bytes can be read, and they're the same
 *   bytes, in the same order.
 *
 * - If we write two batches of five bytes, we can read the same ten bytes in
 *   one read of ten bytes.
 *
 * - If we write ten bytes, we can read the same ten bytes in two reads of
 *   five bytes each.
 *
 * - If we over-fill a buffer (by writing 512k, which we take to be a large
 *   number above default buffer sizes), we block if there is no reader.
 *
 * - That once 512k (ish) is read from the other end, the blocked writer
 *   wakes up.
 *
 * - When a fifo is empty, poll, select, kqueue, and fionread report it is
 *   writable but not readable.
 *
 * - When a fifo has data in it, poll, select, and kqueue report that it is
 *   writable.
 *
 * - XXX: blocked reader semantics?
 *
 * - XXX: event behavior on remote close?
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

static void
cleanfifokq(const char *fifoname, int fd1, int fd2, int fd3)
{

	if (fd3 != -1)
		close(fd3);
	cleanfifo(fifoname, fd1, fd2);
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

static int
set_nonblocking(int fd, const char *testname)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0) {
		warn("%s: fcntl(fd, F_GETFL)", testname);
		return(-1);
	}

	flags |= O_NONBLOCK;

	if (fcntl(fd, F_SETFL, flags) < 0) {
		warn("%s: fcntl(fd, 0x%x)", testname, flags);
		return (-1);
	}

	return (0);
}

static int
set_blocking(int fd, const char *testname)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0) {
		warn("%s: fcntl(fd, F_GETFL)", testname);
		return(-1);
	}

	flags &= ~O_NONBLOCK;

	if (fcntl(fd, F_SETFL, flags) < 0) {
		warn("%s: fcntl(fd, 0x%x)", testname, flags);
		return (-1);
	}

	return (0);
}

/*
 * Drain a file descriptor (fifo) of any readable data.  Note: resets the
 * blocking state.
 */
static int
drain_fd(int fd, const char *testname)
{
	ssize_t len;
	u_char ch;

	if (set_nonblocking(fd, testname) < 0)
		return (-1);

	while ((len = read(fd, &ch, sizeof(ch))) > 0);
	if (len < 0) {
		switch (errno) {
		case EAGAIN:
			return (0);
		default:
			warn("%s: drain_fd: read", testname);
			return (-1);
		}
	}
	warn("%s: drain_fd: read: returned 0 bytes", testname);
	return (-1);
}

/*
 * Simple I/O test: write ten integers, and make sure we get back the same
 * integers in the same order.  This assumes a minimum fifo buffer > 10
 * bytes in order to not block and deadlock.
 */
static void
test_simpleio(void)
{
	int i, reader_fd, writer_fd;
	u_char buffer[10];
	ssize_t len;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", "test_simpleio", &reader_fd, &writer_fd)
	    < 0) {
		warn("test_simpleio: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	for (i = 0; i < 10; i++)
		buffer[i] = i;

	len = write(writer_fd, (char *)buffer, sizeof(buffer));
	if (len < 0) {
		warn("test_simpleio: write");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != sizeof(buffer)) {
		warnx("test_simplio: tried %d but wrote %d", sizeof(buffer),
		    len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	len = read(reader_fd, (char *)buffer, sizeof(buffer));
	if (len < 0) {
		warn("test_simpleio: read");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != sizeof(buffer)) {
		warnx("test_simpleio: tried %d but read %d", sizeof(buffer),
		    len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	for (i = 0; i < 10; i++) {
		if (buffer[i] == i)
			continue;
		warnx("test_simpleio: write byte %d as 0x%02x, but read "
		    "0x%02x", i, i, buffer[i]);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", reader_fd, writer_fd);
}

static int alarm_fired;
/*
 * Non-destructive SIGALRM handler.
 */
static void
sigalarm(int signum)
{

	alarm_fired = 1;
}

/*
 * Wrapper function for write, which uses a timer to interrupt any blocking.
 * Because we can't reliably detect EINTR for blocking I/O, we also track
 * whether or not our timeout fired.
 */
static int __unused
timed_write(int fd, void *data, size_t len, ssize_t *written_lenp,
    int timeout, int *timedoutp, const char *testname)
{
	struct sigaction act, oact;
	ssize_t written_len;
	int error;

	alarm_fired = 0;
	bzero(&act, sizeof(oact));
	act.sa_handler = sigalarm;
	if (sigaction(SIGALRM, &act, &oact) < 0) {
	 	warn("%s: timed_write: sigaction", testname);
		return (-1);
	}
	alarm(timeout);
	written_len = write(fd, data, len);
	error = errno;
	alarm(0);
	if (sigaction(SIGALRM, &oact, NULL) < 0) {
	 	warn("%s: timed_write: sigaction", testname);
		return (-1);
	}
	if (alarm_fired)
		*timedoutp = 1;
	else
		*timedoutp = 0;

	errno = error;
	if (written_len < 0)
		return (-1);
	*written_lenp = written_len;
	return (0);
}

/*
 * Wrapper function for read, which uses a timer to interrupt any blocking.
 * Because we can't reliably detect EINTR for blocking I/O, we also track
 * whether or not our timeout fired.
 */
static int
timed_read(int fd, void *data, size_t len, ssize_t *read_lenp,
    int timeout, int *timedoutp, const char *testname)
{
	struct sigaction act, oact;
	ssize_t read_len;
	int error;

	alarm_fired = 0;
	bzero(&act, sizeof(oact));
	act.sa_handler = sigalarm;
	if (sigaction(SIGALRM, &act, &oact) < 0) {
	 	warn("%s: timed_write: sigaction", testname);
		return (-1);
	}
	alarm(timeout);
	read_len = read(fd, data, len);
	error = errno;
	alarm(0);
	if (sigaction(SIGALRM, &oact, NULL) < 0) {
	 	warn("%s: timed_write: sigaction", testname);
		return (-1);
	}
	if (alarm_fired)
		*timedoutp = 1;
	else
		*timedoutp = 0;

	errno = error;
	if (read_len < 0)
		return (-1);
	*read_lenp = read_len;
	return (0);
}

/*
 * This test operates on blocking and non-blocking fifo file descriptors, in
 * order to determine whether they block at good moments or not.  By good we
 * mean: don't block for non-blocking sockets, and do block for blocking
 * ones, assuming there isn't I/O buffer to satisfy the request.
 *
 * We use a timeout of 5 seconds, concluding that in 5 seconds either all I/O
 * that can take place will, and that if we reach the end of the timeout,
 * then blocking has occured.
 *
 * We assume that the buffer size on a fifo is <512K, and as such, that
 * writing that much data without an active reader will result in blocking.
 */
static void
test_blocking_read_empty(void)
{
	int reader_fd, ret, timedout, writer_fd;
	ssize_t len;
	u_char ch;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_blocking_read_empty: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	/*
	 * Read one byte from an empty blocking fifo, block as there is no
	 * data.
	 */
	if (set_blocking(reader_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	ret = timed_read(reader_fd, &ch, sizeof(ch), &len, 5, &timedout,
	    __func__);
	if (ret != -1) {
		warnx("test_blocking_read_empty: timed_read: returned "
		    "success");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (errno != EINTR) {
		warn("test_blocking_read_empty: timed_read");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	/*
	 * Read one byte from an empty non-blocking fifo, return EAGAIN as
	 * there is no data.
	 */
	if (set_nonblocking(reader_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	ret = timed_read(reader_fd, &ch, sizeof(ch), &len, 5, &timedout,
	    __func__);
	if (ret != -1) {
		warnx("test_blocking_read_empty: timed_read: returned "
		    "success");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (errno != EAGAIN) {
		warn("test_blocking_read_empty: timed_read");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", reader_fd, writer_fd);
}

/*
 * Write one byte to an empty fifo, then try to read one byte and make sure
 * we don't block in either the write or the read.  This tests both for
 * improper blocking in the send and receive code.
 */
static void
test_blocking_one_byte(void)
{
	int reader_fd, ret, timedout, writer_fd;
	ssize_t len;
	u_char ch;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_blocking: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	if (set_blocking(writer_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (set_blocking(reader_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	ch = 0xfe;
	ret = timed_write(writer_fd, &ch, sizeof(ch), &len, 5, &timedout,
	    __func__);
	if (ret < 0) {
		warn("test_blocking_one_byte: timed_write");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != sizeof(ch)) {
		warnx("test_blocking_one_byte: timed_write: tried to write "
		    "%d, wrote %d", sizeof(ch), len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	ch = 0xab;
	ret = timed_read(reader_fd, &ch, sizeof(ch), &len, 5, &timedout,
	    __func__);
	if (ret < 0) {
		warn("test_blocking_one_byte: timed_read");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != sizeof(ch)) {
		warnx("test_blocking_one_byte: timed_read: wanted %d, "
		    "read %d", sizeof(ch), len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (ch != 0xfe) {
		warnx("test_blocking_one_byte: timed_read: expected to read "
		    "0x%02x, read 0x%02x", 0xfe, ch);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", reader_fd, writer_fd);
}

/*
 * Write one byte to an empty fifo, then try to read one byte and make sure
 * we don't get back EAGAIN.
 */
static void
test_nonblocking_one_byte(void)
{
	int reader_fd, ret, timedout, writer_fd;
	ssize_t len;
	u_char ch;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_nonblocking: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	if (set_nonblocking(reader_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	ch = 0xfe;
	ret = timed_write(writer_fd, &ch, sizeof(ch), &len, 5, &timedout,
	    __func__);
	if (ret < 0) {
		warn("test_nonblocking_one_byte: timed_write");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != sizeof(ch)) {
		warnx("test_nonblocking_one_byte: timed_write: tried to write "
		    "%d, wrote %d", sizeof(ch), len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	ch = 0xab;
	ret = timed_read(reader_fd, &ch, sizeof(ch), &len, 5, &timedout,
	    __func__);
	if (ret < 0) {
		warn("test_nonblocking_one_byte: timed_read");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != sizeof(ch)) {
		warnx("test_nonblocking_one_byte: timed_read: wanted %d, read "
		    "%d", sizeof(ch), len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (ch != 0xfe) {
		warnx("test_nonblocking_one_byte: timed_read: expected to read "
		    "0x%02x, read 0x%02x", 0xfe, ch);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", reader_fd, writer_fd);
}

/*
 * First of two test cases involving a 512K buffer: write the buffer into a
 * blocking file descriptor.  We'd like to know it blocks, but the closest we
 * can get is to see if SIGALRM fired during the I/O resulting in a partial
 * write.
 */
static void
test_blocking_partial_write(void)
{
	int reader_fd, ret, timedout, writer_fd;
	u_char *buffer;
	ssize_t len;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_blocking_partial_write: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	if (set_blocking(writer_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	buffer = malloc(512*1024);
	if (buffer == NULL) {
		warn("test_blocking_partial_write: malloc");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	bzero(buffer, 512*1024);

	ret = timed_write(writer_fd, buffer, 512*1024, &len, 5, &timedout,
	    __func__);
	if (ret < 0) {
		warn("test_blocking_partial_write: timed_write");
		free(buffer);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	if (!timedout) {
		warnx("test_blocking_partial_write: timed_write: blocking "
		    "socket didn't time out");
		free(buffer);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	free(buffer);

	if (drain_fd(reader_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", reader_fd, writer_fd);
}

/*
 * Write a 512K buffer to an empty fifo using a non-blocking file descriptor,
 * and make sure it doesn't block.
 */
static void
test_nonblocking_partial_write(void)
{
	int reader_fd, ret, timedout, writer_fd;
	u_char *buffer;
	ssize_t len;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_blocking_partial_write: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	if (set_nonblocking(writer_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	buffer = malloc(512*1024);
	if (buffer == NULL) {
		warn("test_blocking_partial_write: malloc");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	bzero(buffer, 512*1024);

	ret = timed_write(writer_fd, buffer, 512*1024, &len, 5, &timedout,
	    __func__);
	if (ret < 0) {
		warn("test_blocking_partial_write: timed_write");
		free(buffer);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	if (timedout) {
		warnx("test_blocking_partial_write: timed_write: "
		    "non-blocking socket timed out");
		free(buffer);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	if (len == 0 || len >= 512*1024) {
		warnx("test_blocking_partial_write: timed_write: requested "
		    "%d, sent %d", 512*1024, len);
		free(buffer);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	free(buffer);

	if (drain_fd(reader_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", reader_fd, writer_fd);
}

/*
 * test_coalesce_big_read() verifies that data mingles in the fifo across
 * message boundaries by performing two small writes, then a bigger read
 * that should return data from both writes.
 */
static void
test_coalesce_big_read(void)
{
	int i, reader_fd, writer_fd;
	u_char buffer[10];
	ssize_t len;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_coalesce_big_read: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	/* Write five, write five, read ten. */
	for (i = 0; i < 10; i++)
		buffer[i] = i;

	len = write(writer_fd, buffer, 5);
	if (len < 0) {
		warn("test_coalesce_big_read: write 5");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != 5) {
		warnx("test_coalesce_big_read: write 5 wrote %d", len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	len = write(writer_fd, buffer + 5, 5);
	if (len < 0) {
		warn("test_coalesce_big_read: write 5");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != 5) {
		warnx("test_coalesce_big_read: write 5 wrote %d", len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	len = read(reader_fd, buffer, 10);
	if (len < 0) {
		warn("test_coalesce_big_read: read 10");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != 10) {
		warnx("test_coalesce_big_read: read 10 read %d", len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	for (i = 0; i < 10; i++) {
		if (buffer[i] == i)
			continue;
		warnx("test_coalesce_big_read: expected to read 0x%02x, "
		    "read 0x%02x", i, buffer[i]);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", -1, -1);
}

/*
 * test_coalesce_big_write() verifies that data mingles in the fifo across
 * message boundaries by performing one big write, then two smaller reads
 * that should return sequential elements of data from the write.
 */
static void
test_coalesce_big_write(void)
{
	int i, reader_fd, writer_fd;
	u_char buffer[10];
	ssize_t len;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_coalesce_big_write: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	/* Write ten, read five, read five. */
	for (i = 0; i < 10; i++)
		buffer[i] = i;

	len = write(writer_fd, buffer, 10);
	if (len < 0) {
		warn("test_coalesce_big_write: write 10");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != 10) {
		warnx("test_coalesce_big_write: write 10 wrote %d", len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	len = read(reader_fd, buffer, 5);
	if (len < 0) {
		warn("test_coalesce_big_write: read 5");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != 5) {
		warnx("test_coalesce_big_write: read 5 read %d", len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	len = read(reader_fd, buffer + 5, 5);
	if (len < 0) {
		warn("test_coalesce_big_write: read 5");
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (len != 5) {
		warnx("test_coalesce_big_write: read 5 read %d", len);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	for (i = 0; i < 10; i++) {
		if (buffer[i] == i)
			continue;
		warnx("test_coalesce_big_write: expected to read 0x%02x, "
		    "read 0x%02x", i, buffer[i]);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", -1, -1);
}

static int
poll_status(int fd, int *readable, int *writable, int *exception,
    const char *testname)
{	
	struct pollfd fds[1];

	fds[0].fd = fd;
	fds[0].events = POLLIN | POLLOUT | POLLERR;
	fds[0].revents = 0;

	if (poll(fds, 1, 0) < 0) {
		warn("%s: poll", testname);
		return (-1);
	}
	*readable = (fds[0].revents & POLLIN) ? 1 : 0;
	*writable = (fds[0].revents & POLLOUT) ? 1 : 0;
	*exception = (fds[0].revents & POLLERR) ? 1 : 0;
	return (0);
}

static int
select_status(int fd, int *readable, int *writable, int *exception,
    const char *testname)
{
	struct fd_set readfds, writefds, exceptfds;
	struct timeval timeout;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	FD_SET(fd, &readfds);
	FD_SET(fd, &writefds);
	FD_SET(fd, &exceptfds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select(fd+1, &readfds, &writefds, &exceptfds, &timeout) < 0) {
		warn("%s: select", testname);
		return (-1);
	}
	*readable = FD_ISSET(fd, &readfds) ? 1 : 0;
	*writable = FD_ISSET(fd, &writefds) ? 1 : 0;
	*exception = FD_ISSET(fd, &exceptfds) ? 1 : 0;
	return (0);
}

static int
kqueue_setup(int reader_fd, int writer_fd, int *kqueue_fdp,
    const char *testname)
{
	struct kevent kevent_changelist[4], kevent_eventlist[4], *kp;
	struct timespec timeout;
	int i, kqueue_fd, ret;

	kqueue_fd = kqueue();
	if (kqueue_fd < 0) {
		warn("%s: kevent_setup: kqueue", testname);
		return (-1);
	}

	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;

	/*
	 * Add events corresponding to read and write on both descriptors.
	 */
	bzero(&kevent_changelist, sizeof(kevent_changelist));
	EV_SET(&kevent_changelist[0], reader_fd, EVFILT_READ, EV_ADD, 0, 0,
	    0);
	EV_SET(&kevent_changelist[1], reader_fd, EVFILT_WRITE, EV_ADD, 0, 0,
	    0);
	EV_SET(&kevent_changelist[2], writer_fd, EVFILT_READ, EV_ADD, 0, 0,
	    0);
	EV_SET(&kevent_changelist[3], writer_fd, EVFILT_WRITE, EV_ADD, 0, 0,
	    0);

	bzero(&kevent_eventlist, sizeof(kevent_eventlist));
	ret = kevent(kqueue_fd, kevent_changelist, 4, kevent_eventlist, 4,
	    &timeout);
	if (ret < 0) {
		warn("%s: kevent initial register", __func__);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	/*
	 * Verified that the events registered alright.
	 */
	for (i = 0; i < ret; i++) {
		kp = &kevent_eventlist[i];
		if (kp->flags != EV_ERROR)
			continue;
		errno = kp->data;
		err(-1, "%s: kevent register index %d", __func__, i);
	}

	*kqueue_fdp = kqueue_fd;

	return (0);
}

static int
kqueue_status(int kqueue_fd, int fd, int *readable, int *writable,
    int *exception, const char *testname)
{
	struct kevent kevent_eventlist[4], *kp;
	struct timespec timeout;
	int i;

	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;

	if (kevent(kqueue_fd, NULL, 0, kevent_eventlist, 4, &timeout) < 0) {
		warn("%s: kqueue_status: kevent", testname);
		return (-1);
	}

	*readable = *writable = *exception = 0;
	for (i = 0; i < 4; i++) {
		kp = &kevent_eventlist[i];
		if (kp->ident != (u_int)fd)
			continue;
		if (kp->filter == EVFILT_READ)
			*readable = 1;
		if (kp->filter == EVFILT_WRITE)
			*writable = 1;
	}

	return (0);
}

static int
fionread_status(int fd, int *readable, const char *testname)
{
	int i;

	if (ioctl(fd, FIONREAD, &i) < 0) {
		warn("%s: ioctl(FIONREAD)", testname);
		return (-1);
	}

	if (i > 0)
		*readable = 1;
	else
		*readable = 0;
	return (0);
}

/*
 * test_events() uses poll(), select(), and kevent() to query the status of
 * fifo file descriptors and determine whether they match expected state
 * based on earlier semantic tests: specifically, whether or not poll/select/
 * kevent will correctly inform on readable/writable state following I/O.
 *
 * It would be nice to also test status changes as a result of closing of one
 * or another fifo endpoint.
 */
static void
test_events_outofbox(void)
{
	int readable, writable, exception;
	int kqueue_fd, reader_fd, writer_fd;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_events_outofbox: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	if (kqueue_setup(reader_fd, writer_fd, &kqueue_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	/*
	 * Make sure that fresh, out-of-the-box fifo file descriptors have
	 * good initial states.  The reader_fd should have no active state,
	 * since it will not be readable (no data in pipe), writable (it's
	 * a read-only descriptor), and there's no reason for error yet.
	 */
	if (poll_status(reader_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || writable || exception) {
		warnx("test_events_outofbox: reader_fd polls r:%d, w:%d, "
		    "e:%d on create", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (select_status(reader_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || writable || exception) {
		warnx("test_events_outofbox: reader_fd selects r:%d, w:%d, "
		    "e:%d on create", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (kqueue_status(kqueue_fd, reader_fd, &readable, &writable,
	    &exception, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || writable || exception) {
		warnx("test_events_outofbox: reader_fd kevent r:%d, w:%d, "
		    "e:%d on create", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (fionread_status(reader_fd, &readable, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable) {
		warnx("test_events_outofbox: reader_fd fionread r:%d on "
		    "create", readable);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	/*
	 * Make sure that fresh, out-of-the-box fifo file descriptors have
	 * good initial states.  The writer_fd should be ready to write.
	 */
	if (poll_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_outofbox: writer_fd polls r:%d, w:%d, "
		    "e:%d on create", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (select_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_outofbox: writer_fd selects r:%d, w:%d, "
		    "e:%d on create", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (kqueue_status(kqueue_fd, writer_fd, &readable, &writable,
	    &exception, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_outofbox: writer_fd kevent r:%d, w:%d, "
		    "e:%d on create", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (fionread_status(writer_fd, &readable, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable) {
		warnx("test_events_outofbox: writer_fd fionread r:%d on "
		    "create", readable);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
}

static void
test_events_write_read_byte(void)
{
	int readable, writable, exception;
	int kqueue_fd, reader_fd, writer_fd;
	ssize_t len;
	u_char ch;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_events_write_read_byte: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	if (kqueue_setup(reader_fd, writer_fd, &kqueue_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	/*
	 * Write a byte to the fifo, and make sure that the read end becomes
	 * readable, and that the write end remains writable (small write).
	 */
	ch = 0x00;
	len = write(writer_fd, &ch, sizeof(ch));
	if (len < 0) {
		warn("test_events_write_read_byte: write");
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (poll_status(reader_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (!readable || writable || exception) {
		warnx("test_events_write_read_byte: reader_fd polls r:%d, "
		    "w:%d, e:%d after write", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (select_status(reader_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (!readable || writable || exception) {
		warnx("test_events_write_read_byte: reader_fd selects r:%d, "
		    "w:%d, e:%d after write", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (kqueue_status(kqueue_fd, reader_fd, &readable, &writable,
	    &exception, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (!readable || writable || exception) {
		warnx("test_events_write_read_byte: reader_fd kevent r:%d, "
		    "w:%d, e:%d after write", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (fionread_status(reader_fd, &readable, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (!readable) {
		warnx("test_events_outofbox: reader_fd fionread r:%d after "
		    "write", readable);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	/*
	 * Now the writer_fd.
	 */
	if (poll_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_write_read_byte: writer_fd polls r:%d, "
		    "w:%d, e:%d after write", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (select_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_write_read_byte: writer_fd selects r:%d, "
		    "w:%d, e:%d after write", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (kqueue_status(kqueue_fd, writer_fd, &readable, &writable,
	    &exception, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_write_read_byte: writer_fd kevent r:%d, "
		    "w:%d, e:%d after write", readable, writable, exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (fionread_status(writer_fd, &readable, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable) {
		warnx("test_events_outofbox: writer_fd fionread r:%d after "
		    "write", readable);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	/*
	 * Read the byte from the reader_fd, and now confirm that that fifo
	 * becomes unreadable.
	 */
	len = read(reader_fd, &ch, sizeof(ch));
	if (len < 0) {
		warn("test_events_write_read_byte: read");
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (poll_status(reader_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || writable || exception) {
		warnx("test_events_write_read_byte: reader_fd polls r:%d, "
		    "w:%d, e:%d after write+read", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (select_status(reader_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || writable || exception) {
		warnx("test_events_write_read_byte: reader_fd selects r:%d, "
		    "w:%d, e:%d after write+read", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (kqueue_status(kqueue_fd, reader_fd, &readable, &writable,
	    &exception, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || writable || exception) {
		warnx("test_events_write_read_byte: reader_fd kevent r:%d, "
		    "w:%d, e:%d after write+read", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (fionread_status(reader_fd, &readable, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable) {
		warnx("test_events_outofbox: reader_fd fionread r:%d after "
		    "write+read", readable);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	/*
	 * Now the writer_fd.
	 */
	if (poll_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_write_read_byte: writer_fd polls r:%d, "
		    "w:%d, e:%d after write+read", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (select_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_write_read_byte: writer_fd selects r:%d, "
		    "w:%d, e:%d after write+read", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (kqueue_status(kqueue_fd, writer_fd, &readable, &writable,
	    &exception, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_write_read_byte: writer_fd kevent r:%d, "
		    "w:%d, e:%d after write+read", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (fionread_status(writer_fd, &readable, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable) {
		warnx("test_events_outofbox: writer_fd fionread r:%d after "
		    "write+read", readable);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
}

/*
 * Write a 512k buffer to the fifo in non-blocking mode, and make sure that
 * the write end becomes un-writable as a result of a partial write that
 * fills the fifo buffer.
 */
static void
test_events_partial_write(void)
{
	int readable, writable, exception;
	int kqueue_fd, reader_fd, writer_fd;
	u_char *buffer;
	ssize_t len;

	makefifo("testfifo", __func__);
	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd)
	    < 0) {
		warn("test_events_partial_write: openfifo: testfifo");
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	if (kqueue_setup(reader_fd, writer_fd, &kqueue_fd, __func__) < 0) {
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	if (set_nonblocking(writer_fd, "test_events") < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	buffer = malloc(512*1024);
	if (buffer == NULL) {
		warn("test_events_partial_write: malloc");
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}
	bzero(buffer, 512*1024);

	len = write(writer_fd, buffer, 512*1024);
	if (len < 0) {
		warn("test_events_partial_write: write");
		free(buffer);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	free(buffer);

	if (poll_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || writable || exception) {
		warnx("test_events_partial_write: writer_fd polls r:%d, "
		    "w:%d, e:%d after big write", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (select_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || writable || exception) {
		warnx("test_events_partial_write: writer_fd selects r:%d, "
		    "w:%d, e:%d after big write", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (kqueue_status(kqueue_fd, writer_fd, &readable, &writable,
	    &exception, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || writable || exception) {
		warnx("test_events_partial_write: writer_fd kevent r:%d, "
		    "w:%d, e:%d after big write", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (drain_fd(reader_fd, "test_events") < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	/*
	 * Test that the writer_fd has been restored to writable state after
	 * draining.
	 */
	if (poll_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_partial_write: writer_fd polls r:%d, "
		    "w:%d, e:%d after big write + drain", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (select_status(writer_fd, &readable, &writable, &exception,
	    __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_partial_write: writer_fd selects r:%d, "
		    "w:%d, e:%d after big write + drain", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (kqueue_status(kqueue_fd, writer_fd, &readable, &writable,
	    &exception, __func__) < 0) {
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	if (readable || !writable || exception) {
		warnx("test_events_partial_write: writer_fd kevent r:%d, "
		    "w:%d, e:%d after big write + drain", readable, writable,
		    exception);
		cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
		exit(-1);
	}

	cleanfifokq("testfifo", reader_fd, writer_fd, kqueue_fd);
}

int
main(int argc, char *argv[])
{

	strcpy(temp_dir, "/tmp/fifo_io.XXXXXXXXXXX");
	if (mkdtemp(temp_dir) == NULL)
		err(-1, "mkdtemp");
	atexit(atexit_temp_dir);

	if (chdir(temp_dir) < 0)
		err(-1, "chdir %s", temp_dir);

	test_simpleio();
	test_blocking_read_empty();
	test_blocking_one_byte();
	test_nonblocking_one_byte();
	test_blocking_partial_write();
	test_nonblocking_partial_write();
	test_coalesce_big_read();
	test_coalesce_big_write();
	test_events_outofbox();
	test_events_write_read_byte();
	test_events_partial_write();

	return (0);
}
