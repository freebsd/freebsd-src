/*-
 * Copyright (c) 2004 Robert N. M. Watson
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
#include <sys/socket.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*-
 * This test uses UNIX domain socket pairs to perform some basic exercising
 * of kqueue functionality on sockets.  In particular, testing that for read
 * and write filters, we see the correct detection of whether reads and
 * writes should actually be able to occur.
 *
 * TODO:
 * - Test read/write filters for listen/accept sockets.
 * - Handle the XXXRW below regarding datagram sockets.
 * - Test that watermark/buffer size "data" fields returned by kqueue are
 *   correct.
 * - Check that kqueue does something sensible when the remote endpoing is
 *   closed.
 */

static void
fail(int error, const char *func, const char *socktype, const char *rest)
{

	fprintf(stderr, "FAIL\n");

	if (socktype == NULL)
		fprintf(stderr, "%s(): %s\n", func, strerror(error));
	else if (rest == NULL)
		fprintf(stderr, "%s(%s): %s\n", func, socktype,
		    strerror(error));
	else
		fprintf(stderr, "%s(%s, %s): %s\n", func, socktype, rest,
		    strerror(error));
	exit(-1);
}

static void
fail_assertion(const char *func, const char *socktype, const char *rest,
    const char *assertion)
{

	fprintf(stderr, "FAIL\n");

	if (socktype == NULL)
		fprintf(stderr, "%s(): assertion %s failed\n", func,
		    assertion);
	else if (rest == NULL)
		fprintf(stderr, "%s(%s): assertion %s failed\n", func,
		    socktype, assertion);
	else
		fprintf(stderr, "%s(%s, %s): assertion %s failed\n", func,
		    socktype, rest, assertion);
	exit(-1);
}

/*
 * Test read kevent on a socket pair: check to make sure endpoint 0 isn't
 * readable when we start, then write to endpoint 1 and confirm that endpoint
 * 0 is now readable.  Drain the write, then check that it's not readable
 * again.  Use non-blocking kqueue operations and socket operations.
 */
static void
test_evfilt_read(int kq, int fd[2], const char *socktype)
{
	struct timespec ts;
	struct kevent ke;
	ssize_t len;
	char ch;
	int i;

	EV_SET(&ke, fd[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &ke, 1, NULL, 0, NULL) == -1)
		fail(errno, "kevent", socktype, "EVFILT_READ, EV_ADD");

	/*
	 * Confirm not readable to begin with, no I/O yet.
	 */
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	i = kevent(kq, NULL, 0, &ke, 1, &ts);
	if (i == -1)
		fail(errno, "kevent", socktype, "EVFILT_READ");
	if (i != 0)
		fail_assertion("kevent", socktype, "EVFILT_READ",
		    "empty socket unreadable");

	/*
	 * Write a byte to one end.
	 */
	ch = 'a';
	len = write(fd[1], &ch, sizeof(ch));
	if (len == -1)
		fail(errno, "write", socktype, NULL);
	if (len != sizeof(ch))
		fail_assertion("write", socktype, NULL, "write length");

	/*
	 * Other end should now be readable.
	 */
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	i = kevent(kq, NULL, 0, &ke, 1, &ts);
	if (i == -1)
		fail(errno, "kevent", socktype, "EVFILT_READ");
	if (i != 1)
		fail_assertion("kevent", socktype, "EVFILT_READ",
		    "non-empty socket unreadable");

	/*
	 * Read a byte to clear the readable state.
	 */
	len = read(fd[0], &ch, sizeof(ch));
	if (len == -1)
		fail(errno, "read", socktype, NULL);
	if (len != sizeof(ch))
		fail_assertion("read", socktype, NULL, "read length");

	/*
	 * Now re-check for readability.
	 */
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	i = kevent(kq, NULL, 0, &ke, 1, &ts);
	if (i == -1)
		fail(errno, "kevent", socktype, "EVFILT_READ");
	if (i != 0)
		fail_assertion("kevent", socktype, "EVFILT_READ",
		    "empty socket unreadable");

	EV_SET(&ke, fd[0], EVFILT_READ, EV_DELETE, 0, 0, NULL);
	if (kevent(kq, &ke, 1, NULL, 0, NULL) == -1)
		fail(errno, "kevent", socktype, "EVFILT_READ, EV_DELETE");
}

static void
test_evfilt_write(int kq, int fd[2], const char *socktype)
{
	struct timespec ts;
	struct kevent ke;
	ssize_t len;
	char ch;
	int i;

	EV_SET(&ke, fd[0], EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &ke, 1, NULL, 0, NULL) == -1)
		fail(errno, "kevent", socktype, "EVFILT_WRITE, EV_ADD");

	/*
	 * Confirm writable to begin with, no I/O yet.
	 */
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	i = kevent(kq, NULL, 0, &ke, 1, &ts);
	if (i == -1)
		fail(errno, "kevent", socktype, "EVFILT_WRITE");
	if (i != 1)
		fail_assertion("kevent", socktype, "EVFILT_WRITE",
		    "empty socket unwritable");

	/*
	 * Write bytes into the socket until we can't write anymore.
	 */
	ch = 'a';
	while ((len = write(fd[0], &ch, sizeof(ch))) == sizeof(ch)) {};
	if (len == -1 && errno != EAGAIN && errno != ENOBUFS)
		fail(errno, "write", socktype, NULL);
	if (len != -1 && len != sizeof(ch))
		fail_assertion("write", socktype, NULL, "write length");

	/*
	 * Check to make sure the socket is no longer writable.
	 */
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	i = kevent(kq, NULL, 0, &ke, 1, &ts);
	if (i == -1)
		fail(errno, "kevent", socktype, "EVFILT_WRITE");
	if (i != 0)
		fail_assertion("kevent", socktype, "EVFILT_WRITE",
		    "full socket writable");

	EV_SET(&ke, fd[0], EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	if (kevent(kq, &ke, 1, NULL, 0, NULL) == -1)
		fail(errno, "kevent", socktype, "EVFILT_WRITE, EV_DELETE");
}

/*
 * Basic registration exercise for kqueue(2).  Create several types/brands of
 * sockets, and confirm that we can register for various events on them.
 */
int
main(int argc, char *argv[])
{
	int i, kq, sv[2];

	kq = kqueue();
	if (kq == -1)
		fail(errno, "kqueue", NULL, NULL);

	/*
	 * Create a UNIX domain datagram socket, and attach/test/detach a
	 * read filter on it.
	 */
	if (socketpair(PF_UNIX, SOCK_DGRAM, 0, sv) == -1)
		fail(errno, "socketpair", "PF_UNIX, SOCK_DGRAM", NULL);

	i = O_NONBLOCK;
	if (fcntl(sv[0], F_SETFL, &i) != 0)
		fail(errno, "fcntl", "PF_UNIX, SOCK_DGRAM", "O_NONBLOCK");
	if (fcntl(sv[1], F_SETFL, &i) != 0)
		fail(errno, "fcntl", "PF_UNIX, SOCK_DGRAM", "O_NONBLOCK");

	test_evfilt_read(kq, sv, "PF_UNIX, SOCK_DGRAM");

	if (close(sv[0]) == -1)
		fail(errno, "close", "PF_UNIX/SOCK_DGRAM", "sv[0]");
	if (close(sv[1]) == -1)
		fail(errno, "close", "PF_UNIX/SOCK_DGRAM", "sv[1]");

#if 0
	/*
	 * XXXRW: We disable the write test in the case of datagram sockets,
	 * as kqueue can't tell when the remote socket receive buffer is
	 * full, whereas the UNIX domain socket implementation can tell and
	 * returns ENOBUFS.
	 */
	/*
	 * Create a UNIX domain datagram socket, and attach/test/detach a
	 * write filter on it.
	 */
	if (socketpair(PF_UNIX, SOCK_DGRAM, 0, sv) == -1)
		fail(errno, "socketpair", "PF_UNIX, SOCK_DGRAM", NULL);

	i = O_NONBLOCK;
	if (fcntl(sv[0], F_SETFL, &i) != 0)
		fail(errno, "fcntl", "PF_UNIX, SOCK_DGRAM", "O_NONBLOCK");
	if (fcntl(sv[1], F_SETFL, &i) != 0)
		fail(errno, "fcntl", "PF_UNIX, SOCK_DGRAM", "O_NONBLOCK");

	test_evfilt_write(kq, sv, "PF_UNIX, SOCK_DGRAM");

	if (close(sv[0]) == -1)
		fail(errno, "close", "PF_UNIX/SOCK_DGRAM", "sv[0]");
	if (close(sv[1]) == -1)
		fail(errno, "close", "PF_UNIX/SOCK_DGRAM", "sv[1]");
#endif

	/*
	 * Create a UNIX domain stream socket, and attach/test/detach a
	 * read filter on it.
	 */
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) == -1)
		fail(errno, "socketpair", "PF_UNIX, SOCK_STREAM", NULL);

	i = O_NONBLOCK;
	if (fcntl(sv[0], F_SETFL, &i) != 0)
		fail(errno, "fcntl", "PF_UNIX, SOCK_STREAM", "O_NONBLOCK");
	if (fcntl(sv[1], F_SETFL, &i) != 0)
		fail(errno, "fcntl", "PF_UNIX, SOCK_STREAM", "O_NONBLOCK");

	test_evfilt_read(kq, sv, "PF_UNIX, SOCK_STREAM");

	if (close(sv[0]) == -1)
		fail(errno, "close", "PF_UNIX/SOCK_STREAM", "sv[0]");
	if (close(sv[1]) == -1)
		fail(errno, "close", "PF_UNIX/SOCK_STREAM", "sv[1]");

	/*
	 * Create a UNIX domain stream socket, and attach/test/detach a
	 * write filter on it.
	 */
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) == -1)
		fail(errno, "socketpair", "PF_UNIX, SOCK_STREAM", NULL);

	i = O_NONBLOCK;
	if (fcntl(sv[0], F_SETFL, &i) != 0)
		fail(errno, "fcntl", "PF_UNIX, SOCK_STREAM", "O_NONBLOCK");
	if (fcntl(sv[1], F_SETFL, &i) != 0)
		fail(errno, "fcntl", "PF_UNIX, SOCK_STREAM", "O_NONBLOCK");

	test_evfilt_write(kq, sv, "PF_UNIX, SOCK_STREAM");

	if (close(sv[0]) == -1)
		fail(errno, "close", "PF_UNIX/SOCK_STREAM", "sv[0]");
	if (close(sv[1]) == -1)
		fail(errno, "close", "PF_UNIX/SOCK_STREAM", "sv[1]");

	if (close(kq) == -1)
		fail(errno, "close", "kq", NULL);

	printf("PASS\n");
	return (0);
}
