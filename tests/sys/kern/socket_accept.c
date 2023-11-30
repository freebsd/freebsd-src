/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Gleb Smirnoff <glebius@FreeBSD.org>
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
#include <fcntl.h>
#include <string.h>

#include <atf-c.h>

static int
tcp4_listensock(struct sockaddr_in *sin)
{
	int l;

	ATF_REQUIRE((l = socket(PF_INET, SOCK_STREAM, 0)) > 0);
	ATF_REQUIRE(setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &(socklen_t){1},
	    sizeof(int)) == 0);
	*sin = (struct sockaddr_in){
		.sin_len = sizeof(sin),
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	ATF_REQUIRE(bind(l, (struct sockaddr *)sin, sizeof(*sin)) == 0);
	ATF_REQUIRE(getsockname(l, (struct sockaddr *)sin,
	    &(socklen_t){ sizeof(*sin) }) == 0);
	ATF_REQUIRE(listen(l, -1) == 0);

	return (l);
}

static int
tcp4_clientsock(struct sockaddr_in *sin)
{
	int s;

	ATF_REQUIRE((s = socket(PF_INET, SOCK_STREAM, 0)) > 0);
	ATF_REQUIRE(connect(s, (struct sockaddr *)sin, sizeof(*sin)) == 0);

	return (s);
}

ATF_TC_WITHOUT_HEAD(tcp4_zerolen);
ATF_TC_BODY(tcp4_zerolen, tc)
{
	static char canary[sizeof(struct sockaddr_in)] =
	    { [0 ... sizeof(struct sockaddr_in) - 1] = 0xa };
	struct sockaddr_in sin, ret;
	socklen_t salen;
	int l;

	l = tcp4_listensock(&sin);
	(void )tcp4_clientsock(&sin);

	memcpy(&ret, &canary, sizeof(ret));
	salen = 0;
	ATF_REQUIRE(accept(l, (struct sockaddr *)&ret, &salen) > 0);
	ATF_REQUIRE(memcmp(&ret, &canary, sizeof(ret)) == 0);
	ATF_REQUIRE(salen == sizeof(struct sockaddr_in));
	/* Note: Linux will block for connection here, we fail immediately. */
	ATF_REQUIRE(accept(l, (struct sockaddr *)&ret, NULL) == -1);
	ATF_REQUIRE(errno == EFAULT);
}

ATF_TC_WITHOUT_HEAD(tcp4);
ATF_TC_BODY(tcp4, tc)
{
	struct sockaddr_in sin, ret;
	socklen_t salen;
	int l, s;

	l = tcp4_listensock(&sin);
	s = tcp4_clientsock(&sin);

	salen = sizeof(struct sockaddr_in) + 2;
	ATF_REQUIRE(accept(l, (struct sockaddr *)&ret, &salen) > 0);
	ATF_REQUIRE(salen == sizeof(struct sockaddr_in));
	ATF_REQUIRE(getsockname(s, (struct sockaddr *)&sin,
	    &(socklen_t){ sizeof(sin) }) == 0);
	ATF_REQUIRE(memcmp(&ret, &sin, sizeof(sin)) == 0);
}

ATF_TC_WITHOUT_HEAD(tcp4_noaddr);
ATF_TC_BODY(tcp4_noaddr, tc)
{
	struct sockaddr_in sin;
	int l;

	l = tcp4_listensock(&sin);
	(void )tcp4_clientsock(&sin);

	ATF_REQUIRE(accept(l, NULL, NULL) > 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, tcp4);
	ATF_TP_ADD_TC(tp, tcp4_noaddr);
	ATF_TP_ADD_TC(tp, tcp4_zerolen);

	return (atf_no_error());
}
