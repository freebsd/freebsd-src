/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Bjoern A. Zeeb
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
__FBSDID("$FreeBSD$");

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <poll.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(socket_afinet);
ATF_TC_BODY(socket_afinet, tc)
{
	int sd;

	sd = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_CHECK(sd >= 0);

	close(sd);
}

ATF_TC_WITHOUT_HEAD(socket_afinet_bind_zero);
ATF_TC_BODY(socket_afinet_bind_zero, tc)
{
	int sd, rc;
	struct sockaddr_in sin;

	if (atf_tc_get_config_var_as_bool_wd(tc, "ci", false))
		atf_tc_skip("doesn't work when mac_portacl(4) loaded (https://bugs.freebsd.org/238781)");

	sd = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_CHECK(sd >= 0);

	bzero(&sin, sizeof(sin));
	/*
	 * For AF_INET we do not check the family in in_pcbbind_setup(9),
	 * sa_len gets set from the syscall argument in getsockaddr(9),
	 * so we bind to 0:0.
	 */
	rc = bind(sd, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);

	close(sd);
}

ATF_TC_WITHOUT_HEAD(socket_afinet_bind_ok);
ATF_TC_BODY(socket_afinet_bind_ok, tc)
{
	int sd, rc;
	struct sockaddr_in sin;

	sd = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_CHECK(sd >= 0);

	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	rc = bind(sd, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);

	close(sd);
}

ATF_TC_WITHOUT_HEAD(socket_afinet_poll_no_rdhup);
ATF_TC_BODY(socket_afinet_poll_no_rdhup, tc)
{
	int ss, ss2, cs, rc;
	struct sockaddr_in sin;
	socklen_t slen;
	struct pollfd pfd;
	int one = 1;

	/* Verify that we don't expose POLLRDHUP if not requested. */

	/* Server setup. */
	ss = socket(PF_INET, SOCK_STREAM, 0);
	ATF_CHECK(ss >= 0);
	rc = setsockopt(ss, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
	ATF_CHECK_EQ(0, rc);
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	rc = bind(ss, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);
	rc = listen(ss, 1);
	ATF_CHECK_EQ(0, rc);
	slen = sizeof(sin);
	rc = getsockname(ss, (struct sockaddr *)&sin, &slen);
	ATF_CHECK_EQ(0, rc);

	/* Client connects, server accepts. */
	cs = socket(PF_INET, SOCK_STREAM, 0);
	ATF_CHECK(cs >= 0);
	rc = connect(cs, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);
	ss2 = accept(ss, NULL, NULL);
	ATF_CHECK(ss2 >= 0);

	/* Server can write, sees only POLLOUT. */
	pfd.fd = ss2;
	pfd.events = POLLIN | POLLOUT;
	rc = poll(&pfd, 1, 0);
	ATF_CHECK_EQ(1, rc);
	ATF_CHECK_EQ(POLLOUT, pfd.revents);

	/* Client closes socket! */
	rc = close(cs);
	ATF_CHECK_EQ(0, rc);

	/*
	 * Server now sees POLLIN, but not POLLRDHUP because we didn't ask.
	 * Need non-zero timeout to wait for the FIN to arrive and trigger the
	 * socket to become readable.
	 */
	pfd.fd = ss2;
	pfd.events = POLLIN;
	rc = poll(&pfd, 1, 60000);
	ATF_CHECK_EQ(1, rc);
	ATF_CHECK_EQ(POLLIN, pfd.revents);

	close(ss2);
	close(ss);
}

ATF_TC_WITHOUT_HEAD(socket_afinet_poll_rdhup);
ATF_TC_BODY(socket_afinet_poll_rdhup, tc)
{
	int ss, ss2, cs, rc;
	struct sockaddr_in sin;
	socklen_t slen;
	struct pollfd pfd;
	char buffer;
	int one = 1;

	/* Verify that server sees POLLRDHUP if it asks for it. */

	/* Server setup. */
	ss = socket(PF_INET, SOCK_STREAM, 0);
	ATF_CHECK(ss >= 0);
	rc = setsockopt(ss, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
	ATF_CHECK_EQ(0, rc);
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	rc = bind(ss, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);
	rc = listen(ss, 1);
	ATF_CHECK_EQ(0, rc);
	slen = sizeof(sin);
	rc = getsockname(ss, (struct sockaddr *)&sin, &slen);
	ATF_CHECK_EQ(0, rc);

	/* Client connects, server accepts. */
	cs = socket(PF_INET, SOCK_STREAM, 0);
	ATF_CHECK(cs >= 0);
	rc = connect(cs, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);
	ss2 = accept(ss, NULL, NULL);
	ATF_CHECK(ss2 >= 0);

	/* Server can write, so sees POLLOUT. */
	pfd.fd = ss2;
	pfd.events = POLLIN | POLLOUT | POLLRDHUP;
	rc = poll(&pfd, 1, 0);
	ATF_CHECK_EQ(1, rc);
	ATF_CHECK_EQ(POLLOUT, pfd.revents);

	/* Client writes two bytes, server reads only one of them. */
	rc = write(cs, "xx", 2);
	ATF_CHECK_EQ(2, rc);
	rc = read(ss2, &buffer, 1);
	ATF_CHECK_EQ(1, rc);

	/* Server can read, so sees POLLIN. */
	pfd.fd = ss2;
	pfd.events = POLLIN | POLLOUT | POLLRDHUP;
	rc = poll(&pfd, 1, 0);
	ATF_CHECK_EQ(1, rc);
	ATF_CHECK_EQ(POLLIN | POLLOUT, pfd.revents);

	/* Client closes socket! */
	rc = close(cs);
	ATF_CHECK_EQ(0, rc);

	/*
	 * Server sees Linux-style POLLRDHUP.  Note that this is the case even
	 * though one byte of data remains unread.
	 *
	 * This races against the delivery of FIN caused by the close() above.
	 * Sometimes (more likely when run under truss or if another system
	 * call is added in between) it hits the path where sopoll_generic()
	 * immediately sees SBS_CANTRCVMORE, and sometimes it sleeps with flag
	 * SB_SEL so that it's woken up almost immediately and runs again,
	 * which is why we need a non-zero timeout here.
	 */
	pfd.fd = ss2;
	pfd.events = POLLRDHUP;
	rc = poll(&pfd, 1, 60000);
	ATF_CHECK_EQ(1, rc);
	ATF_CHECK_EQ(POLLRDHUP, pfd.revents);

	close(ss2);
	close(ss);
}

ATF_TC_WITHOUT_HEAD(socket_afinet_stream_reconnect);
ATF_TC_BODY(socket_afinet_stream_reconnect, tc)
{
	struct sockaddr_in sin;
	socklen_t slen;
	int ss, cs, rc;

	/*
	 * Make sure that an attempt to connect(2) a connected or disconnected
	 * stream socket fails with EISCONN.
	 */

	/* Server setup. */
	ss = socket(PF_INET, SOCK_STREAM, 0);
	ATF_CHECK(ss >= 0);
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	rc = bind(ss, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);
	rc = listen(ss, 1);
	ATF_CHECK_EQ(0, rc);
	slen = sizeof(sin);
	rc = getsockname(ss, (struct sockaddr *)&sin, &slen);
	ATF_CHECK_EQ(0, rc);

	/* Client connects, shuts down. */
	cs = socket(PF_INET, SOCK_STREAM, 0);
	ATF_CHECK(cs >= 0);
	rc = connect(cs, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(0, rc);
	rc = shutdown(cs, SHUT_RDWR);
	ATF_CHECK_EQ(0, rc);

	/* A subsequent connect(2) fails with EISCONN. */
	rc = connect(cs, (struct sockaddr *)&sin, sizeof(sin));
	ATF_CHECK_EQ(-1, rc);
	ATF_CHECK_EQ(errno, EISCONN);

	rc = close(cs);
	ATF_CHECK_EQ(0, rc);
	rc = close(ss);
	ATF_CHECK_EQ(0, rc);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, socket_afinet);
	ATF_TP_ADD_TC(tp, socket_afinet_bind_zero);
	ATF_TP_ADD_TC(tp, socket_afinet_bind_ok);
	ATF_TP_ADD_TC(tp, socket_afinet_poll_no_rdhup);
	ATF_TP_ADD_TC(tp, socket_afinet_poll_rdhup);
	ATF_TP_ADD_TC(tp, socket_afinet_stream_reconnect);

	return atf_no_error();
}
