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
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(tcp_implied_connect);
ATF_TC_BODY(tcp_implied_connect, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
	};
	const char buf[] = "hello";
	char repl[sizeof(buf)];
	socklen_t len;
	int s, c, a;

	ATF_REQUIRE(s = socket(PF_INET, SOCK_STREAM, 0));
	ATF_REQUIRE(c = socket(PF_INET, SOCK_STREAM, 0));

	ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	len = sizeof(sin);
	ATF_REQUIRE(getsockname(s, (struct sockaddr *)&sin, &len) == 0);
	ATF_REQUIRE(listen(s, -1) == 0);
#if 0
	/*
	 * The disabled code is that you would normally do.
	 */
	ATF_REQUIRE(connect(c, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(send(c, &buf, sizeof(buf), 0) == sizeof(buf));
#else
	/*
	 * And this is implied connect.
	 */
	ATF_REQUIRE(sendto(c, &buf, sizeof(buf), 0, (struct sockaddr *)&sin,
	    sizeof(sin)) == sizeof(buf));
#endif

	ATF_REQUIRE((a = accept(s, NULL, NULL)) != 1);
	ATF_REQUIRE(recv(a, &repl, sizeof(repl), 0) == sizeof(buf));
	ATF_REQUIRE(strcmp(buf, repl) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, tcp_implied_connect);

	return (atf_no_error());
}
