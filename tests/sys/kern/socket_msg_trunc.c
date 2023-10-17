/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Alexander V. Chernikov
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

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/ktrace.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>

#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

static void
check_recvmsg(int cs, int ss, struct sockaddr *sa, const size_t sizes[],
    size_t nsizes)
{
	char buf[4096];

	memset(buf, 0xFF, sizeof(buf));
	for (size_t i = 0; i < nsizes; i++) {
		ssize_t rc;
		size_t sz = sizes[i];
		char tbuf[1];

		rc = sendto(cs, buf, sz, 0, sa, sa->sa_len);
		ATF_REQUIRE_MSG(rc != -1, "sendto failed: %s", strerror(errno));
		ATF_REQUIRE((size_t)rc == sz);

		rc = recv(ss, NULL, 0, MSG_PEEK | MSG_TRUNC);
		ATF_REQUIRE_MSG(rc >= 0, "recv failed: %s", strerror(errno));
		ATF_REQUIRE((size_t)rc == sz);

		rc = recv(ss, tbuf, sizeof(tbuf), MSG_PEEK | MSG_TRUNC);
		ATF_REQUIRE_MSG(rc >= 0, "recv failed: %s", strerror(errno));
		ATF_REQUIRE((size_t)rc == sz);

		rc = recv(ss, tbuf, sizeof(tbuf), MSG_TRUNC);
		ATF_REQUIRE_MSG(rc >= 0, "recv failed: %s", strerror(errno));
		ATF_REQUIRE((size_t)rc == sz);
	}

	ATF_REQUIRE(close(cs) == 0);
	ATF_REQUIRE(close(ss) == 0);
}

ATF_TC_WITHOUT_HEAD(recv_trunc_afinet_udp);
ATF_TC_BODY(recv_trunc_afinet_udp, tc)
{
	struct sockaddr_in sin;
	struct sockaddr *sa;
	int ss, cs, rc;

	ss = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(ss >= 0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(6666);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sa = (struct sockaddr *)&sin;
	rc = bind(ss, sa, sa->sa_len);
	ATF_REQUIRE_MSG(rc == 0, "bind failed: %s", strerror(errno));

	cs = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(cs >= 0);

	size_t sizes[] = {80, 255, 256, 1024, 4096, 9000};
	check_recvmsg(cs, ss, sa, sizes, nitems(sizes));
}

ATF_TC_WITHOUT_HEAD(recv_trunc_afinet6_udp);
ATF_TC_BODY(recv_trunc_afinet6_udp, tc)
{
	struct sockaddr_in6 sin6;
	struct sockaddr *sa;
	int cs, ss, rc;

	ss = socket(PF_INET6, SOCK_DGRAM, 0);
	ATF_REQUIRE(ss >= 0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_port = htons(6666);
	const struct in6_addr in6loopback = IN6ADDR_LOOPBACK_INIT;
	sin6.sin6_addr = in6loopback;
	sa = (struct sockaddr *)&sin6;
	rc = bind(ss, sa, sa->sa_len);
	ATF_REQUIRE_MSG(rc == 0, "bind failed: %s", strerror(errno));

	cs = socket(PF_INET6, SOCK_DGRAM, 0);
	ATF_REQUIRE(cs >= 0);

	size_t sizes[] = {80, 255, 256, 1024, 4096, 9000};
	check_recvmsg(cs, ss, sa, sizes, nitems(sizes));
}

ATF_TC_WITHOUT_HEAD(recv_trunc_afunix_dgram);
ATF_TC_BODY(recv_trunc_afunix_dgram, tc)
{
	struct sockaddr_un sun;
	struct sockaddr *sa;
	int ss, cs, rc;

	ss = socket(PF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(ss >= 0);

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, "test_check_recvmsg_socket", sizeof(sun.sun_path));
	sun.sun_len = sizeof(sun);
	sa = (struct sockaddr *)&sun;
	rc = bind(ss, sa, sa->sa_len);
	ATF_REQUIRE_MSG(rc == 0, "bind failed: %s", strerror(errno));

	cs = socket(PF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(cs >= 0);

	size_t sizes[] = {80, 255, 256, 1024, 2000};
	check_recvmsg(cs, ss, sa, sizes, nitems(sizes));
}

ATF_TC_WITHOUT_HEAD(recv_trunc_afunix_seqpacket);
ATF_TC_BODY(recv_trunc_afunix_seqpacket, tc)
{
	struct sockaddr_un sun;
	struct sockaddr *sa;
	int ss, nss, cs, rc;

	ss = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(ss >= 0);

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, "test_check_recvmsg_socket", sizeof(sun.sun_path));
	sun.sun_len = sizeof(sun);
	sa = (struct sockaddr *)&sun;
	rc = bind(ss, sa, sa->sa_len);
	ATF_REQUIRE_MSG(rc == 0, "bind failed: %s", strerror(errno));
	rc = listen(ss, 1);
	ATF_REQUIRE_MSG(rc == 0, "listen failed: %s", strerror(errno));

	cs = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(cs >= 0);
	rc = connect(cs, sa, sa->sa_len);
	ATF_REQUIRE_MSG(rc == 0, "connect failed: %s", strerror(errno));
	nss = accept(ss, NULL, NULL);
	ATF_REQUIRE(nss >= 0);

	size_t sizes[] = {80, 255, 256, 1024, 2000};
	check_recvmsg(cs, nss, sa, sizes, nitems(sizes));

	ATF_REQUIRE(close(ss) == 0);
}

/*
 * Exercise the case where ktrace was used to dump a truncated buffer.
 */
ATF_TC_WITHOUT_HEAD(recvmsg_trunc_ktrace_uio);
ATF_TC_BODY(recvmsg_trunc_ktrace_uio, tc)
{
	struct ktr_header ktr;
	struct msghdr msg;
	struct iovec iov;
	const char *tracepath;
	char buf[128];
	ssize_t nbytes;
	int error, fd, sd[2];

	tracepath = "ktrace";

	error = socketpair(AF_UNIX, SOCK_DGRAM, 0, sd);
	ATF_REQUIRE(error == 0);

	memset(buf, 0, sizeof(buf));
	nbytes = send(sd[0], buf, sizeof(buf), 0);
	ATF_REQUIRE_MSG(nbytes >= 0, "send failed: %s", strerror(errno));
	ATF_REQUIRE((size_t)nbytes == sizeof(buf));

	fd = open(tracepath, O_RDWR | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE_MSG(fd >= 0, "open failed: %s", strerror(errno));
	error = ktrace(tracepath, KTROP_SET, KTRFAC_GENIO, getpid());
	ATF_REQUIRE_MSG(error == 0,
	    "ktrace(SET) failed: %s", strerror(errno));

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf) - 1; /* truncate */
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	nbytes = recvmsg(sd[1], &msg, MSG_TRUNC);
	ATF_REQUIRE_MSG(nbytes >= 0, "recvmsg failed: %s", strerror(errno));
	ATF_REQUIRE((size_t)nbytes == sizeof(buf));
	ATF_REQUIRE((msg.msg_flags & MSG_TRUNC) != 0);

	error = ktrace(tracepath, KTROP_CLEARFILE, 0, getpid());
	ATF_REQUIRE_MSG(error == 0,
	    "ktrace(CLEARFILE) failed: %s", strerror(errno));

	nbytes = read(fd, &ktr, sizeof(ktr));
	ATF_REQUIRE_MSG(nbytes >= 0, "read failed: %s", strerror(errno));
	ATF_REQUIRE((size_t)nbytes == sizeof(ktr));
	ATF_REQUIRE((ktr.ktr_type & ~KTR_TYPE) == KTR_GENIO);

	ATF_REQUIRE(close(fd) == 0);
	ATF_REQUIRE(close(sd[0]) == 0);
	ATF_REQUIRE(close(sd[1]) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, recv_trunc_afinet_udp);
	ATF_TP_ADD_TC(tp, recv_trunc_afinet6_udp);
	ATF_TP_ADD_TC(tp, recv_trunc_afunix_dgram);
	ATF_TP_ADD_TC(tp, recv_trunc_afunix_seqpacket);
	ATF_TP_ADD_TC(tp, recvmsg_trunc_ktrace_uio);

	return (atf_no_error());
}
