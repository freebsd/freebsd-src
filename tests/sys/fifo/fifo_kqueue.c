/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Jan Kokemüller
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/event.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(fifo_kqueue__writes);
ATF_TC_BODY(fifo_kqueue__writes, tc)
{
	int p[2] = { -1, -1 };

	ATF_REQUIRE(mkfifo("testfifo", 0600) == 0);

	ATF_REQUIRE((p[0] = open("testfifo",
	    O_RDONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);
	ATF_REQUIRE((p[1] = open("testfifo",
	    O_WRONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);

	int kq = kqueue();
	ATF_REQUIRE(kq >= 0);

	struct kevent kev[32];
	EV_SET(&kev[0], p[1], EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, 0);
	EV_SET(&kev[1], p[1], EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);

	ATF_REQUIRE(kevent(kq, kev, 2, NULL, 0, NULL) == 0);

	/* A new writer should immediately get a EVFILT_WRITE event. */

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[1]);
	ATF_REQUIRE(kev[0].filter == EVFILT_WRITE);
	ATF_REQUIRE(kev[0].flags == EV_CLEAR);
	ATF_REQUIRE(kev[0].fflags == 0);
	ATF_REQUIRE(kev[0].data == 16384);
	ATF_REQUIRE(kev[0].udata == 0);

	/* Filling up the pipe should make the EVFILT_WRITE disappear. */

	char c = 0;
	ssize_t r;
	while ((r = write(p[1], &c, 1)) == 1) {
	}
	ATF_REQUIRE(r < 0);
	ATF_REQUIRE(errno == EAGAIN || errno == EWOULDBLOCK);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	/* Reading (PIPE_BUF - 1) bytes will not trigger a EVFILT_WRITE yet. */

	for (int i = 0; i < PIPE_BUF - 1; ++i) {
		ATF_REQUIRE(read(p[0], &c, 1) == 1);
	}

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	/* Reading one additional byte triggers the EVFILT_WRITE. */

	ATF_REQUIRE(read(p[0], &c, 1) == 1);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[1]);
	ATF_REQUIRE(kev[0].filter == EVFILT_WRITE);
	ATF_REQUIRE(kev[0].flags == EV_CLEAR);
	ATF_REQUIRE(kev[0].fflags == 0);
	ATF_REQUIRE(kev[0].data == PIPE_BUF);
	ATF_REQUIRE(kev[0].udata == 0);

	/*
	 * Reading another byte triggers the EVFILT_WRITE again with a changed
	 * 'data' field.
	 */

	ATF_REQUIRE(read(p[0], &c, 1) == 1);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[1]);
	ATF_REQUIRE(kev[0].filter == EVFILT_WRITE);
	ATF_REQUIRE(kev[0].flags == EV_CLEAR);
	ATF_REQUIRE(kev[0].fflags == 0);
	ATF_REQUIRE(kev[0].data == PIPE_BUF + 1);
	ATF_REQUIRE(kev[0].udata == 0);

	/*
	 * Closing the read end should make a EV_EOF appear but leave the 'data'
	 * field unchanged.
	 */

	ATF_REQUIRE(close(p[0]) == 0);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev), NULL) == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[1]);
	ATF_REQUIRE(kev[0].filter == EVFILT_WRITE);
	ATF_REQUIRE(kev[0].flags == (EV_CLEAR | EV_EOF));
	ATF_REQUIRE(kev[0].fflags == 0);
	ATF_REQUIRE(kev[0].data == PIPE_BUF + 1);
	ATF_REQUIRE(kev[0].udata == 0);

	ATF_REQUIRE(close(kq) == 0);
	ATF_REQUIRE(close(p[1]) == 0);
}

ATF_TC_WITHOUT_HEAD(fifo_kqueue__connecting_reader);
ATF_TC_BODY(fifo_kqueue__connecting_reader, tc)
{
	int p[2] = { -1, -1 };

	ATF_REQUIRE(mkfifo("testfifo", 0600) == 0);

	ATF_REQUIRE((p[0] = open("testfifo",
	    O_RDONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);
	ATF_REQUIRE((p[1] = open("testfifo",
	    O_WRONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);

	int kq = kqueue();
	ATF_REQUIRE(kq >= 0);

	struct kevent kev[32];
	EV_SET(&kev[0], p[1], EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, 0);
	EV_SET(&kev[1], p[1], EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);

	ATF_REQUIRE(kevent(kq, kev, 2, NULL, 0, NULL) == 0);

	/* A new writer should immediately get a EVFILT_WRITE event. */

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[1]);
	ATF_REQUIRE(kev[0].filter == EVFILT_WRITE);
	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	/*
	 * Filling the pipe, reading (PIPE_BUF + 1) bytes, then closing the
	 * read end leads to a EVFILT_WRITE with EV_EOF set.
	 */

	char c = 0;
	ssize_t r;
	while ((r = write(p[1], &c, 1)) == 1) {
	}
	ATF_REQUIRE(r < 0);
	ATF_REQUIRE(errno == EAGAIN || errno == EWOULDBLOCK);

	for (int i = 0; i < PIPE_BUF + 1; ++i) {
		ATF_REQUIRE(read(p[0], &c, 1) == 1);
	}

	ATF_REQUIRE(close(p[0]) == 0);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev), NULL) == 1);
	ATF_REQUIRE(kev[0].filter == EVFILT_WRITE);
	ATF_REQUIRE((kev[0].flags & EV_EOF) != 0);
	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	/* Opening the reader again must trigger the EVFILT_WRITE. */

	ATF_REQUIRE((p[0] = open("testfifo",
	    O_RDONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);

	r = kevent(kq, NULL, 0, kev, nitems(kev), &(struct timespec) { 1, 0 });
	ATF_REQUIRE(r == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[1]);
	ATF_REQUIRE(kev[0].filter == EVFILT_WRITE);
	ATF_REQUIRE(kev[0].flags == EV_CLEAR);
	ATF_REQUIRE(kev[0].fflags == 0);
	ATF_REQUIRE(kev[0].data == PIPE_BUF + 1);
	ATF_REQUIRE(kev[0].udata == 0);
	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	ATF_REQUIRE(close(kq) == 0);
	ATF_REQUIRE(close(p[0]) == 0);
	ATF_REQUIRE(close(p[1]) == 0);
}

/* Check that EVFILT_READ behaves sensibly on a FIFO reader. */
ATF_TC_WITHOUT_HEAD(fifo_kqueue__reads);
ATF_TC_BODY(fifo_kqueue__reads, tc)
{
	struct kevent kev[32];
	ssize_t bytes, i, n;
	int kq, p[2];
	char c;

	ATF_REQUIRE(mkfifo("testfifo", 0600) == 0);

	ATF_REQUIRE((p[0] = open("testfifo",
	    O_RDONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);
	ATF_REQUIRE((p[1] = open("testfifo",
	    O_WRONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);

	bytes = 0;
	c = 0;
	while ((n = write(p[1], &c, 1)) == 1)
		bytes++;
	ATF_REQUIRE(n < 0);
	ATF_REQUIRE(errno == EAGAIN || errno == EWOULDBLOCK);
	ATF_REQUIRE(bytes > 1);

	for (i = 0; i < bytes / 2; i++)
		ATF_REQUIRE(read(p[0], &c, 1) == 1);
	bytes -= i;

	kq = kqueue();
	ATF_REQUIRE(kq >= 0);

	EV_SET(&kev[0], p[0], EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);

	ATF_REQUIRE(kevent(kq, kev, 1, NULL, 0, NULL) == 0);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec){ 0, 0 }) == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[0]);
	ATF_REQUIRE(kev[0].filter == EVFILT_READ);
	ATF_REQUIRE(kev[0].flags == EV_CLEAR);
	ATF_REQUIRE(kev[0].fflags == 0);
	ATF_REQUIRE(kev[0].data == bytes);
	ATF_REQUIRE(kev[0].udata == 0);

	while (bytes-- > 0)
		ATF_REQUIRE(read(p[0], &c, 1) == 1);
	n = read(p[0], &c, 1);
	ATF_REQUIRE(n < 0);
	ATF_REQUIRE(errno == EAGAIN || errno == EWOULDBLOCK);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	ATF_REQUIRE(close(kq) == 0);
	ATF_REQUIRE(close(p[0]) == 0);
	ATF_REQUIRE(close(p[1]) == 0);
}

ATF_TC_WITHOUT_HEAD(fifo_kqueue__read_eof_wakeups);
ATF_TC_BODY(fifo_kqueue__read_eof_wakeups, tc)
{
	int p[2] = { -1, -1 };

	ATF_REQUIRE(mkfifo("testfifo", 0600) == 0);

	ATF_REQUIRE((p[0] = open("testfifo",
	    O_RDONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);
	ATF_REQUIRE((p[1] = open("testfifo",
	    O_WRONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);

	int kq = kqueue();
	ATF_REQUIRE(kq >= 0);

	struct kevent kev[32];

	EV_SET(&kev[0], p[0], EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);
	ATF_REQUIRE(kevent(kq, kev, 1, NULL, 0, NULL) == 0);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	/*
	 * Closing the writer must trigger a EVFILT_READ edge with EV_EOF set.
	 */

	ATF_REQUIRE(close(p[1]) == 0);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[0]);
	ATF_REQUIRE(kev[0].filter == EVFILT_READ);
	ATF_REQUIRE(kev[0].flags == (EV_EOF | EV_CLEAR));
	ATF_REQUIRE(kev[0].fflags == 0);
	ATF_REQUIRE(kev[0].data == 0);
	ATF_REQUIRE(kev[0].udata == 0);

	/*
	 * Trying to read from a closed pipe should not trigger EVFILT_READ
	 * edges.
	 */

	char c;
	ATF_REQUIRE(read(p[0], &c, 1) == 0);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	ATF_REQUIRE(close(kq) == 0);
	ATF_REQUIRE(close(p[0]) == 0);
}

ATF_TC_WITHOUT_HEAD(fifo_kqueue__read_eof_state_when_reconnecting);
ATF_TC_BODY(fifo_kqueue__read_eof_state_when_reconnecting, tc)
{
	int p[2] = { -1, -1 };

	ATF_REQUIRE(mkfifo("testfifo", 0600) == 0);

	ATF_REQUIRE((p[0] = open("testfifo",
	    O_RDONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);
	ATF_REQUIRE((p[1] = open("testfifo",
	    O_WRONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);

	int kq = kqueue();
	ATF_REQUIRE(kq >= 0);

	struct kevent kev[32];

	EV_SET(&kev[0], p[0], EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);
	ATF_REQUIRE(kevent(kq, kev, 1, NULL, 0, NULL) == 0);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	/*
	 * Closing the writer must trigger a EVFILT_READ edge with EV_EOF set.
	 */

	ATF_REQUIRE(close(p[1]) == 0);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[0]);
	ATF_REQUIRE(kev[0].filter == EVFILT_READ);
	ATF_REQUIRE(kev[0].flags == (EV_EOF | EV_CLEAR));
	ATF_REQUIRE(kev[0].fflags == 0);
	ATF_REQUIRE(kev[0].data == 0);
	ATF_REQUIRE(kev[0].udata == 0);

	/* A new reader shouldn't see the EOF flag. */

	{
		int new_reader;
		ATF_REQUIRE((new_reader = open("testfifo",
		    O_RDONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);

		int new_kq = kqueue();
		ATF_REQUIRE(new_kq >= 0);

		struct kevent new_kev[32];
		EV_SET(&new_kev[0], new_reader, EVFILT_READ, EV_ADD | EV_CLEAR,
		    0, 0, 0);
		ATF_REQUIRE(kevent(new_kq, new_kev, 1, NULL, 0, NULL) == 0);

		ATF_REQUIRE(kevent(new_kq, NULL, 0, new_kev, nitems(new_kev),
		    &(struct timespec) { 0, 0 }) == 0);

		ATF_REQUIRE(close(new_kq) == 0);
		ATF_REQUIRE(close(new_reader) == 0);
	}

	/*
	 * Simply reopening the writer does not trigger the EVFILT_READ again --
	 * EV_EOF should be cleared, but there is no data yet so the filter
	 * does not trigger.
	 */

	ATF_REQUIRE((p[1] = open("testfifo",
	    O_WRONLY | O_CLOEXEC | O_NONBLOCK)) >= 0);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 0);

	/* Writing a byte should trigger a EVFILT_READ. */

	char c = 0;
	ATF_REQUIRE(write(p[1], &c, 1) == 1);

	ATF_REQUIRE(kevent(kq, NULL, 0, kev, nitems(kev),
	    &(struct timespec) { 0, 0 }) == 1);
	ATF_REQUIRE(kev[0].ident == (uintptr_t)p[0]);
	ATF_REQUIRE(kev[0].filter == EVFILT_READ);
	ATF_REQUIRE(kev[0].flags == EV_CLEAR);
	ATF_REQUIRE(kev[0].fflags == 0);
	ATF_REQUIRE(kev[0].data == 1);
	ATF_REQUIRE(kev[0].udata == 0);

	ATF_REQUIRE(close(kq) == 0);
	ATF_REQUIRE(close(p[0]) == 0);
	ATF_REQUIRE(close(p[1]) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, fifo_kqueue__writes);
	ATF_TP_ADD_TC(tp, fifo_kqueue__connecting_reader);
	ATF_TP_ADD_TC(tp, fifo_kqueue__reads);
	ATF_TP_ADD_TC(tp, fifo_kqueue__read_eof_wakeups);
	ATF_TP_ADD_TC(tp, fifo_kqueue__read_eof_state_when_reconnecting);

	return atf_no_error();
}
