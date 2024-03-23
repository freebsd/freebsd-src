/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Gleb Smirnoff <glebius@FreeBSD.org>
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * Create a pair of UDP sockets.  The first one is bound to a local
 * address and the second one is connected to it.
 */
static void
udp_socketpair(int *s)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
	};
	socklen_t slen = sizeof(sin);
	int b, c;

	ATF_REQUIRE((b = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE((c = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(bind(b, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(getsockname(b, (struct sockaddr *)&sin, &slen) == 0);
	ATF_REQUIRE(connect(c, (struct sockaddr *)&sin, sizeof(sin)) == 0);

	s[0] = b;
	s[1] = c;
}

/*
 * Check MSG_TRUNC.
 */
ATF_TC_WITHOUT_HEAD(trunc);
ATF_TC_BODY(trunc, tc)
{
	char sbuf[] = "Hello, peer!", rbuf[sizeof(sbuf)];
	struct iovec iov = {
		.iov_base = sbuf,
		.iov_len = sizeof(sbuf),
	};
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	int s[2];
	u_int n;

	udp_socketpair(s);

	ATF_REQUIRE(sendmsg(s[1], &msg, 0) == sizeof(sbuf));
	n = (arc4random() % (sizeof(sbuf) - 1)) + 1;
	iov.iov_base = rbuf;
	iov.iov_len = n;
	ATF_REQUIRE(recvmsg(s[0], &msg, 0) == n);
	ATF_REQUIRE(msg.msg_flags == MSG_TRUNC);
	ATF_REQUIRE(strncmp(sbuf, rbuf, n) == 0);
	iov.iov_len = sizeof(rbuf);
	ATF_REQUIRE(recvmsg(s[0], &msg, MSG_DONTWAIT) == -1);
	ATF_REQUIRE(errno == EAGAIN);

	close(s[0]);
	close(s[1]);
}

/*
 * Check MSG_PEEK.
 */
ATF_TC_WITHOUT_HEAD(peek);
ATF_TC_BODY(peek, tc)
{
	char sbuf[] = "Hello, peer!", rbuf[sizeof(sbuf)];
	struct iovec iov = {
		.iov_base = sbuf,
		.iov_len = sizeof(sbuf),
	};
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	int s[2];
	u_int n;

	udp_socketpair(s);

	ATF_REQUIRE(sendmsg(s[1], &msg, 0) == sizeof(sbuf));
	iov.iov_base = rbuf;
	for (int i = 0; i < 10; i++) {
		n = (arc4random() % sizeof(sbuf)) + 1;
		iov.iov_len = n;
		ATF_REQUIRE(recvmsg(s[0], &msg, MSG_PEEK) == n);
		if (n < sizeof(sbuf))
			ATF_REQUIRE(msg.msg_flags == (MSG_PEEK | MSG_TRUNC));
		else
			ATF_REQUIRE(msg.msg_flags == MSG_PEEK);
		ATF_REQUIRE(strncmp(sbuf, rbuf, n) == 0);
	}

	close(s[0]);
	close(s[1]);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, trunc);
	ATF_TP_ADD_TC(tp, peek);

	return (atf_no_error());
}
