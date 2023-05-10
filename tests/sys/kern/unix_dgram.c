/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Gleb Smirnoff <glebius@FreeBSD.org>
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

#include <sys/time.h>
#include <sys/event.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <aio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

#include <atf-c.h>

static struct itimerval itv = {
	.it_interval = { 0, 0 },
	.it_value = { 1, 0 },	/* one second */
};
static sig_atomic_t timer_done = 0;
static void
sigalarm(int sig __unused)
{

	timer_done = 1;
}

static struct sigaction sigact = {
	.sa_handler = sigalarm,
};

static struct sockaddr_un sun = {
	.sun_family = AF_LOCAL,
	.sun_len = sizeof(sun),
	.sun_path = "unix_dgram_listener",
};

/*
 * Fill socket to a state when next send(len) would fail.
 *
 * Note that every datagram is prepended with sender address,
 * size of struct sockaddr.
 */
static void
fill(int fd, void *buf, ssize_t len)
{
	unsigned long recvspace;
	size_t llen = sizeof(unsigned long);
	ssize_t sent;

	ATF_REQUIRE(sysctlbyname("net.local.dgram.recvspace", &recvspace,
	    &llen, NULL, 0) == 0);
	for (sent = 0;
	    sent + len + sizeof(struct sockaddr) < recvspace;
	    sent += len + sizeof(struct sockaddr))
		ATF_REQUIRE(send(fd, buf, len, 0) == len);
}

ATF_TC_WITHOUT_HEAD(basic);
ATF_TC_BODY(basic, tc)
{
	struct msghdr msg;
	struct iovec iov[1];
	unsigned long maxdgram;
	size_t llen = sizeof(unsigned long);
	int fd[2];
	char *buf;

	/* Allocate and initialize:
	 * - fd[0] to send, fd[1] to receive
	 * - buf[maxdgram] for data
	 */
	ATF_REQUIRE(sysctlbyname("net.local.dgram.maxdgram", &maxdgram,
	    &llen, NULL, 0) == 0);
	ATF_REQUIRE(socketpair(PF_UNIX, SOCK_DGRAM, 0, fd) != -1);
	buf = malloc(maxdgram + 1);
	ATF_REQUIRE(buf);
	msg = (struct msghdr ){
		.msg_iov = iov,
		.msg_iovlen = 1,
	};
	iov[0] = (struct iovec ){
		.iov_base = buf,
	};

	/* Fail to send > maxdgram. */
	ATF_REQUIRE(send(fd[0], buf, maxdgram + 1, 0) == -1);
	ATF_REQUIRE(errno == EMSGSIZE);

	/* Send maxdgram. */
	ATF_REQUIRE(send(fd[0], buf, maxdgram, 0) == (ssize_t)maxdgram);

	/* Exercise MSG_PEEK, full and truncated.. */
	ATF_REQUIRE(recv(fd[1], buf, maxdgram, MSG_PEEK) == (ssize_t)maxdgram);
	iov[0].iov_len = 42;
	ATF_REQUIRE(recvmsg(fd[1], &msg, MSG_PEEK) == 42);
	ATF_REQUIRE(msg.msg_flags == (MSG_PEEK | MSG_TRUNC));

	/* Receive maxdgram. */
	iov[0].iov_len = maxdgram;
	ATF_REQUIRE(recvmsg(fd[1], &msg, 0) == (ssize_t)maxdgram);
	ATF_REQUIRE(msg.msg_flags == 0);

	/* Receive truncated message. */
	ATF_REQUIRE(send(fd[0], buf, maxdgram, 0) == (ssize_t)maxdgram);
	iov[0].iov_len = maxdgram / 2;
	ATF_REQUIRE(recvmsg(fd[1], &msg, 0) == (ssize_t)maxdgram / 2);
	ATF_REQUIRE(msg.msg_flags == MSG_TRUNC);

	/* Empty: block. */
	ATF_REQUIRE(sigaction(SIGALRM, &sigact, NULL) == 0);
	ATF_REQUIRE(timer_done == 0);
	ATF_REQUIRE(setitimer(ITIMER_REAL, &itv, NULL) == 0);
	ATF_REQUIRE(recv(fd[1], buf, maxdgram, 0) == -1);
	ATF_REQUIRE(errno == EINTR);
	ATF_REQUIRE(timer_done == 1);

	/* Don't block with MSG_DONTWAIT. */
	ATF_REQUIRE(recv(fd[1], buf, maxdgram, MSG_DONTWAIT) == -1);
	ATF_REQUIRE(errno == EAGAIN);

	/* Don't block with O_NONBLOCK. */
	ATF_REQUIRE(fcntl(fd[1], F_SETFL, O_NONBLOCK) != -1);
	ATF_REQUIRE(recv(fd[1], buf, maxdgram, 0) == -1);
	ATF_REQUIRE(errno == EAGAIN);

	/* Fail with ENOBUFS on full socket. */
	fill(fd[0], buf, maxdgram);
	ATF_REQUIRE(send(fd[0], buf, maxdgram, 0) == -1);
	ATF_REQUIRE(errno == ENOBUFS);

	/*
	 * Fail with ENOBUFS with O_NONBLOCK set, too. See 71e70c25c00
	 * for explanation why this behavior needs to be preserved.
	 */
	ATF_REQUIRE(fcntl(fd[0], F_SETFL, O_NONBLOCK) != -1);
	ATF_REQUIRE(send(fd[0], buf, maxdgram, 0) == -1);
	ATF_REQUIRE(errno == ENOBUFS);

	/* Remote side closed -> ECONNRESET. */
	close(fd[1]);
	ATF_REQUIRE(send(fd[0], buf, maxdgram, 0) == -1);
	ATF_REQUIRE(errno == ECONNRESET);
}

ATF_TC_WITHOUT_HEAD(one2many);
ATF_TC_BODY(one2many, tc)
{
	int one, many[3], two;
#define	BUFSIZE	1024
	char buf[BUFSIZE], goodboy[BUFSIZE], flooder[BUFSIZE], notconn[BUFSIZE];

	/* Establish one to many connection. */
	ATF_REQUIRE((one = socket(PF_UNIX, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(bind(one, (struct sockaddr *)&sun, sizeof(sun)) == 0);
	/* listen(2) shall fail. */
	ATF_REQUIRE(listen(one, -1) != 0);
	for (int i = 0; i < 3; i++) {
		ATF_REQUIRE((many[i] = socket(PF_UNIX, SOCK_DGRAM, 0)) > 0);
		ATF_REQUIRE(connect(many[i], (struct sockaddr *)&sun,
		    sizeof(sun)) == 0);
	}

	/* accept() on UNIX/DGRAM is invalid. */
	ATF_REQUIRE(accept(one, NULL, NULL) == -1);
	ATF_REQUIRE(errno == EINVAL);

	/*
	 * Connecting a bound socket to self: a strange, useless, but
	 * historically existing edge case that is not explicitly described
	 * in SuS, neither is forbidden there. Works on FreeBSD and Linux.
	 */
	ATF_REQUIRE(connect(one, (struct sockaddr *)&sun, sizeof(sun)) == 0);
	ATF_REQUIRE(send(one, buf, 42, 0) == 42);
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == 42);

	/*
	 * Interaction between concurrent senders. New feature in FreeBSD 14.
	 *
	 * One sender can not fill the receive side.  Other senders can
	 * continue operation.  Senders who don't fill their buffers are
	 * prioritized over flooders.  Connected senders are prioritized over
	 * unconnected.
	 *
	 * Disconnecting a sender that has queued data optionally preserves
	 * the data.  Allow the data to migrate to peers buffer only if the
	 * latter is empty.  Otherwise discard it, to prevent against
	 * connect-fill-close attack.
	 */
#define	FLOODER	13	/* for connected flooder on many[0] */
#define	GOODBOY	42	/* for a good boy on many[1] */
#define	NOTCONN	66	/* for sendto(2) via two */
	goodboy[0] = GOODBOY;
	flooder[0] = FLOODER;
	notconn[0] = NOTCONN;

	/* Connected priority over sendto(2). */
	ATF_REQUIRE((two = socket(PF_UNIX, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(sendto(two, notconn, BUFSIZE, 0, (struct sockaddr *)&sun,
	    sizeof(sun)) == BUFSIZE);
	ATF_REQUIRE(send(many[1], goodboy, BUFSIZE, 0) == BUFSIZE);
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE(buf[0] == GOODBOY);	/* message from good boy comes first */
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE(buf[0] == NOTCONN);	/* only then message from sendto(2) */

	/* Casual sender priority over a flooder. */
	fill(many[0], flooder, sizeof(flooder));
	ATF_REQUIRE(send(many[0], flooder, BUFSIZE, 0) == -1);
	ATF_REQUIRE(errno == ENOBUFS);
	ATF_REQUIRE(send(many[1], goodboy, BUFSIZE, 0) == BUFSIZE);
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE(buf[0] == GOODBOY);	/* message from good boy comes first */
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE(buf[0] == FLOODER);	/* only then message from flooder */

	/* Once seen, a message can't be deprioritized by any other message. */
	ATF_REQUIRE(recv(one, buf, sizeof(buf), MSG_PEEK) == sizeof(buf));
	ATF_REQUIRE(buf[0] == FLOODER); /* message from the flooder seen */
	ATF_REQUIRE(send(many[1], goodboy, BUFSIZE, 0) == BUFSIZE);
	ATF_REQUIRE(recv(one, buf, sizeof(buf), MSG_PEEK) == sizeof(buf));
	ATF_REQUIRE(buf[0] == FLOODER); /* should be the same message */
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE(buf[0] == FLOODER); /* now we read it out... */
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE(buf[0] == GOODBOY); /* ... and next one is the good boy */

	/* Disconnect in presence of data from not connected. */
	ATF_REQUIRE(sendto(two, notconn, BUFSIZE, 0, (struct sockaddr *)&sun,
	    sizeof(sun)) == BUFSIZE);
	close(many[0]);
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE(buf[0] == NOTCONN);	/* message from sendto() */
	ATF_REQUIRE(recv(one, buf, sizeof(buf), MSG_DONTWAIT) == -1);
	ATF_REQUIRE(errno == EAGAIN);	/* data from many[0] discarded */

	/* Disconnect in absence of data from not connected. */
	ATF_REQUIRE(send(many[1], goodboy, BUFSIZE, 0) == BUFSIZE);
	close(many[1]);
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE(buf[0] == GOODBOY);	/* message from many[1] preserved */

	/* Check that nothing leaks on close(2). */
	ATF_REQUIRE(send(many[2], buf, 42, 0) == 42);
	ATF_REQUIRE(send(many[2], buf, 42, 0) == 42);
	ATF_REQUIRE(recv(one, buf, sizeof(buf), MSG_PEEK) == 42);
	ATF_REQUIRE(sendto(two, notconn, 42, 0, (struct sockaddr *)&sun,
	    sizeof(sun)) == 42);
	close(one);
}

/*
 * Check that various mechanism report socket as readable and having
 * 42 bytes of data.
 */
static void
test42(int fd)
{

	/* ioctl(FIONREAD) */
	int data;

	ATF_REQUIRE(ioctl(fd, FIONREAD, &data) != -1);
	ATF_REQUIRE(data == 42);

	/* select(2) */
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	ATF_REQUIRE(select(fd + 1, &rfds, NULL, NULL, NULL) == 1);
	ATF_REQUIRE(FD_ISSET(fd, &rfds));

	/* kevent(2) */
	struct kevent ev;
	int kq;

	ATF_REQUIRE((kq = kqueue()) != -1);
	EV_SET(&ev, fd, EVFILT_READ, EV_ADD, NOTE_LOWAT, 41, NULL);
	ATF_REQUIRE(kevent(kq, &ev, 1, NULL, 0, NULL) == 0);
	ATF_REQUIRE(kevent(kq, NULL, 0, &ev, 1, NULL) == 1);
	ATF_REQUIRE(ev.filter == EVFILT_READ);
	ATF_REQUIRE(ev.data == 42);

	/* aio(4) */
	char buf[50];
	struct aiocb aio = {
		.aio_nbytes = 50,
		.aio_fildes = fd,
		.aio_buf = buf,
	}, *aiop;

	ATF_REQUIRE(aio_read(&aio) == 0);
	ATF_REQUIRE(aio_waitcomplete(&aiop, NULL) == 42);
	ATF_REQUIRE(aiop == &aio);
}

/*
 * Send data and control in connected & unconnected mode and check that
 * various event mechanisms see the data, but don't count control bytes.
 */
ATF_TC_WITHOUT_HEAD(event);
ATF_TC_BODY(event, tc)
{
	int fd[2];
	char buf[50];
	struct iovec iov = {
		.iov_base = buf,
		.iov_len = 42,
	};
	struct cmsghdr cmsg = {
		.cmsg_len = CMSG_LEN(0),
		.cmsg_level = SOL_SOCKET,
		.cmsg_type = SCM_TIMESTAMP,
	};
	struct msghdr msghdr = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = &cmsg,
		.msg_controllen = CMSG_LEN(0),
	};

	/* Connected socket */
	ATF_REQUIRE(socketpair(PF_UNIX, SOCK_DGRAM, 0, fd) != -1);
	ATF_REQUIRE(sendmsg(fd[0], &msghdr, 0) == 42);
	test42(fd[1]);
	close(fd[0]);
	close(fd[1]);

	/* Not-connected send */
	ATF_REQUIRE((fd[0] = socket(PF_UNIX, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE((fd[1] = socket(PF_UNIX, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(bind(fd[0], (struct sockaddr *)&sun, sizeof(sun)) == 0);
	ATF_REQUIRE(sendto(fd[1], buf, 42, 0, (struct sockaddr *)&sun,
	    sizeof(sun)) == 42);
	test42(fd[0]);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, basic);
	ATF_TP_ADD_TC(tp, one2many);
	ATF_TP_ADD_TC(tp, event);

	return (atf_no_error());
}
