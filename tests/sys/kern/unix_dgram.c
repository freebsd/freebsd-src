/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/un.h>
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

	/* Fail with EAGAIN with O_NONBLOCK set. */
	ATF_REQUIRE(fcntl(fd[0], F_SETFL, O_NONBLOCK) != -1);
	ATF_REQUIRE(send(fd[0], buf, maxdgram, 0) == -1);
	ATF_REQUIRE(errno == EAGAIN);

	/* Remote side closed -> ECONNRESET. */
	close(fd[1]);
	ATF_REQUIRE(send(fd[0], buf, maxdgram, 0) == -1);
	ATF_REQUIRE(errno == ECONNRESET);
}

ATF_TC_WITHOUT_HEAD(one2many);
ATF_TC_BODY(one2many, tc)
{
	struct sockaddr_un sun;
	const char *path = "unix_dgram_listener";
	int one, many[2], two;
	char buf[1024];

	/* Establish one to many connection. */
	ATF_REQUIRE((one = socket(PF_UNIX, SOCK_DGRAM, 0)) > 0);
	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	sun.sun_len = sizeof(sun);
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	ATF_REQUIRE(bind(one, (struct sockaddr *)&sun, sizeof(sun)) == 0);
	/* listen(2) shall fail. */
	ATF_REQUIRE(listen(one, -1) != 0);
	for (int i = 0; i < 2; i++) {
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
	 * Sending from an unconnected socket to a bound socket.  Connection is
	 * created for the duration of the syscall.
	 */
	ATF_REQUIRE((two = socket(PF_UNIX, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(sendto(two, buf, 43, 0, (struct sockaddr *)&sun,
	    sizeof(sun)) == 43);
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == 43);

	/* One sender can fill the receive side.
	 * Current behavior which needs improvement.
	 */
	fill(many[0], buf, sizeof(buf));
	ATF_REQUIRE(send(many[1], buf, sizeof(buf), 0) == -1);
	ATF_REQUIRE(errno == ENOBUFS);
	ATF_REQUIRE(recv(one, buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE(send(many[1], buf, sizeof(buf), 0) == sizeof(buf));
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, basic);
	ATF_TP_ADD_TC(tp, one2many);

	return (atf_no_error());
}
