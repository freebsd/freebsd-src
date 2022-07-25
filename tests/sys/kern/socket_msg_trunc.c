/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <poll.h>

#include <atf-c.h>

static void
check_recvmsg(const char *test_name)
{
	int ss, cs, rc;
	struct sockaddr *sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_un saun;
	int *sizes, sizes_count;
	int one = 1;


	if (!strcmp(test_name, "udp")) {
		ss = socket(PF_INET, SOCK_DGRAM, 0);
		ATF_CHECK(ss >= 0);
		rc = setsockopt(ss, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
		ATF_CHECK_EQ(0, rc);
		bzero(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		sin.sin_port = htons(6666);
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		sa = (struct sockaddr *)&sin;
		rc = bind(ss, sa, sa->sa_len);
		ATF_CHECK_EQ(0, rc);

		cs = socket(PF_INET, SOCK_DGRAM, 0);
		ATF_CHECK(cs >= 0);
		int inet_sizes[] = {80, 255, 256, 1024, 4096, 9000};
		sizes_count = sizeof(inet_sizes) / sizeof(int);
		sizes = malloc(sizeof(inet_sizes));
		memcpy(sizes, inet_sizes, sizeof(inet_sizes));

	} else if (!strcmp(test_name, "udp6")) {
		ss = socket(PF_INET6, SOCK_DGRAM, 0);
		ATF_CHECK(ss >= 0);
		rc = setsockopt(ss, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
		ATF_CHECK_EQ(0, rc);
		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_port = htons(6666);
		const struct in6_addr in6loopback = IN6ADDR_LOOPBACK_INIT;
		sin6.sin6_addr = in6loopback;
		sa = (struct sockaddr *)&sin6;
		rc = bind(ss, sa, sa->sa_len);
		ATF_CHECK_EQ(0, rc);

		cs = socket(PF_INET6, SOCK_DGRAM, 0);
		ATF_CHECK(cs >= 0);
		int inet_sizes[] = {80, 255, 256, 1024, 4096, 9000};
		sizes_count = sizeof(inet_sizes) / sizeof(int);
		sizes = malloc(sizeof(inet_sizes));
		memcpy(sizes, inet_sizes, sizeof(inet_sizes));

	} else if (!strcmp(test_name, "unix")) {
		const char *PATH = "/tmp/test_check_recvmsg_socket";
		ss = socket(PF_UNIX, SOCK_DGRAM, 0);
		ATF_CHECK(ss >= 0);
		rc = setsockopt(ss, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
		ATF_CHECK_EQ(0, rc);
		bzero(&saun, sizeof(saun));
		saun.sun_family = AF_UNIX;
		strcpy(saun.sun_path, PATH);
		saun.sun_len = sizeof(saun);
		sa = (struct sockaddr *)&saun;
		unlink(PATH);
		rc = bind(ss, sa, sa->sa_len);
		ATF_CHECK_EQ(0, rc);

		cs = socket(PF_UNIX, SOCK_DGRAM, 0);
		ATF_CHECK(cs >= 0);
		int unix_sizes[] = {80, 255, 256, 1024, 2000};
		sizes_count = sizeof(unix_sizes) / sizeof(int);
		sizes = malloc(sizeof(unix_sizes));
		memcpy(sizes, unix_sizes, sizeof(unix_sizes));
	} else
		return;

	char buf[4096];
	memset(buf, 0xFF, sizeof(buf));
	for (int i = 0; i < sizes_count; i++) {
		int sz = sizes[i];
		char tbuf[1];
		rc = sendto(cs, buf, sz, 0, sa, sa->sa_len);
		ATF_REQUIRE_EQ(rc, sz);

		rc = recv(ss, NULL, 0, MSG_PEEK | MSG_TRUNC);
		ATF_CHECK_EQ(rc, sz);

		rc = recv(ss, tbuf, sizeof(tbuf), MSG_PEEK | MSG_TRUNC);
		ATF_CHECK_EQ(rc, sz);

		rc = recv(ss, tbuf, sizeof(tbuf), MSG_TRUNC);
		ATF_CHECK_EQ(rc, sz);
	}

	close(ss);
	close(cs);
}

ATF_TC_WITHOUT_HEAD(socket_afinet_udp_recv_trunc);
ATF_TC_BODY(socket_afinet_udp_recv_trunc, tc)
{
	check_recvmsg("udp");
}

ATF_TC_WITHOUT_HEAD(socket_afinet6_udp_recv_trunc);
ATF_TC_BODY(socket_afinet6_udp_recv_trunc, tc)
{
	check_recvmsg("udp6");
}

ATF_TC_WITHOUT_HEAD(socket_afunix_recv_trunc);
ATF_TC_BODY(socket_afunix_recv_trunc, tc)
{
	check_recvmsg("unix");
}


ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, socket_afinet_udp_recv_trunc);
	ATF_TP_ADD_TC(tp, socket_afinet6_udp_recv_trunc);
	ATF_TP_ADD_TC(tp, socket_afunix_recv_trunc);

	return atf_no_error();
}
