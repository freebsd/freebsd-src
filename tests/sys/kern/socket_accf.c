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

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>

#include <atf-c.h>

static int
listensock(struct sockaddr_in *sin)
{
	int l;

	ATF_REQUIRE((l = socket(PF_INET, SOCK_STREAM, 0)) > 0);
	ATF_REQUIRE(fcntl(l, F_SETFL, O_NONBLOCK) != -1);
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
clientsock(struct sockaddr_in *sin)
{
	int s;

	ATF_REQUIRE((s = socket(PF_INET, SOCK_STREAM, 0)) > 0);
	ATF_REQUIRE(connect(s, (struct sockaddr *)sin, sizeof(*sin)) == 0);

	return (s);
}

static void
accfon(int l, struct accept_filter_arg *af)
{

	if (setsockopt(l, SOL_SOCKET, SO_ACCEPTFILTER, af, sizeof(*af)) != 0) {
		if (errno == ENOENT)
			atf_tc_skip("Accept filter %s not loaded in kernel",
			    af->af_name);
		else
			atf_tc_fail("setsockopt(SO_ACCEPTFILTER): %s",
			    strerror(errno));
	}
}

/*
 * XXX: return from send(2) on a localhost connection doesn't guarantee that
 * netisr has fully processed and delivered the data to the remote local
 * socket.  Sleep a fraction of second to "guarantee" that it did.
 */
static ssize_t
usend(int s, const void *msg, size_t len)
{
	ssize_t rv;

	rv = send(s, msg, len, 0);
	usleep(100000);
	return (rv);
}

ATF_TC_WITHOUT_HEAD(data);
ATF_TC_BODY(data, tc)
{
	struct accept_filter_arg afa = {
		.af_name = "dataready"
	};
	struct sockaddr_in sin;
	int l, s, a;

	l = listensock(&sin);
	accfon(l, &afa);
	s = clientsock(&sin);
	ATF_REQUIRE(accept(l, NULL, 0) == -1);
	ATF_REQUIRE(errno == EAGAIN);
	ATF_REQUIRE(usend(s, "foo", sizeof("foo")) == sizeof("foo"));
	ATF_REQUIRE((a = accept(l, NULL, 0)) > 0);
}

ATF_TC_WITHOUT_HEAD(http);
ATF_TC_BODY(http, tc)
{
	struct accept_filter_arg afa = {
		.af_name = "httpready"
	};
	struct sockaddr_in sin;
	int l, s, a;

	l = listensock(&sin);
	accfon(l, &afa);
	s = clientsock(&sin);

	/* 1) No data. */
	ATF_REQUIRE(accept(l, NULL, 0) == -1);
	ATF_REQUIRE(errno == EAGAIN);

	/* 2) Data, that doesn't look like HTTP. */
	ATF_REQUIRE(usend(s, "foo", sizeof("foo")) == sizeof("foo"));
	ATF_REQUIRE((a = accept(l, NULL, 0)) > 0);

	close(s);
	close(a);

#define	CHUNK1	"GET / "
#define	CHUNK2	"HTTP/1.0\r\n\n"
#define	LEN(c)	(sizeof(c) - 1)

	/* 3) Partial HTTP. */
	s = clientsock(&sin);
	ATF_REQUIRE(usend(s, CHUNK1, LEN(CHUNK1)) == LEN(CHUNK1));
	ATF_REQUIRE(accept(l, NULL, 0) == -1);
	ATF_REQUIRE(errno == EAGAIN);

	/* 4) Complete HTTP. */
	ATF_REQUIRE(usend(s, CHUNK2, LEN(CHUNK2)) == LEN(CHUNK2));
	ATF_REQUIRE((a = accept(l, NULL, 0)) > 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, data);
	ATF_TP_ADD_TC(tp, http);

	return (atf_no_error());
}
