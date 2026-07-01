/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Bjoern A. Zeeb
 * Copyright (c) 2024 Stormshield
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
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(basic);
ATF_TC_BODY(basic, tc)
{
	int sd;

	sd = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_CHECK(sd >= 0);

	close(sd);
}

ATF_TC_WITHOUT_HEAD(bind_zero);
ATF_TC_BODY(bind_zero, tc)
{
	int sd, rc;
	struct sockaddr_in sin;

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

ATF_TC_WITHOUT_HEAD(bind_ok);
ATF_TC_BODY(bind_ok, tc)
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

ATF_TC_WITHOUT_HEAD(poll_no_rdhup);
ATF_TC_BODY(poll_no_rdhup, tc)
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

ATF_TC_WITHOUT_HEAD(poll_rdhup);
ATF_TC_BODY(poll_rdhup, tc)
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

ATF_TC_WITHOUT_HEAD(stream_reconnect);
ATF_TC_BODY(stream_reconnect, tc)
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

/*
 * Make sure that unprivileged users can't set the IP_BINDANY or IPV6_BINDANY
 * socket options.
 */
ATF_TC(bindany);
ATF_TC_HEAD(bindany, tc)
{
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(bindany, tc)
{
	int s;

	s = socket(AF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(s >= 0);
	ATF_REQUIRE_ERRNO(EPERM,
	    setsockopt(s, IPPROTO_IP, IP_BINDANY, &(int){1}, sizeof(int)) ==
	    -1);
	ATF_REQUIRE(close(s) == 0);

	s = socket(AF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(s >= 0);
	ATF_REQUIRE_ERRNO(EPERM,
	    setsockopt(s, IPPROTO_IP, IP_BINDANY, &(int){1}, sizeof(int)) ==
	    -1);
	ATF_REQUIRE(close(s) == 0);

	s = socket(AF_INET6, SOCK_STREAM, 0);
	ATF_REQUIRE(s >= 0);
	ATF_REQUIRE_ERRNO(EPERM,
	    setsockopt(s, IPPROTO_IPV6, IPV6_BINDANY, &(int){1}, sizeof(int)) ==
	    -1);
	ATF_REQUIRE(close(s) == 0);

	s = socket(AF_INET6, SOCK_DGRAM, 0);
	ATF_REQUIRE(s >= 0);
	ATF_REQUIRE_ERRNO(EPERM,
	    setsockopt(s, IPPROTO_IPV6, IPV6_BINDANY, &(int){1}, sizeof(int)) ==
	    -1);
	ATF_REQUIRE(close(s) == 0);
}

/*
 * Bind a socket to the specified address, optionally dropping privileges and
 * setting one of the SO_REUSE* options first.
 *
 * Expected returns for different test case scenarios are: 1) successful
 * immediate bind(2), successful bind(2) after setting specified SO_REUSE*
 * socket option, and bind(2) failed with EADDRINUSE.
 */
static enum bind_res {
	BIND_FAILED = 0,
	SETEUID_FAIL = 1,
	SOCKET_FAIL = 2,
	BIND_INSTANT_SUCCESS = 3,
	BIND_BADERR1 = 4,
	SETSOCKOPT_FAIL = 5,
	BIND_REUSE_SUCCESS = 6,
	BIND_BADERR2 = 7,
}
child_bind(const atf_tc_t *tc, int type, struct sockaddr *sa, int opt,
    bool unpriv)
{
	const char *user;
	pid_t child;

	if (unpriv) {
		if (!atf_tc_has_config_var(tc, "unprivileged_user"))
			atf_tc_skip("unprivileged_user not set");
		user = atf_tc_get_config_var(tc, "unprivileged_user");
	} else {
		user = NULL;
	}

	child = fork();
	ATF_REQUIRE(child != -1);
	if (child == 0) {
		int s;

		if (user != NULL) {
			struct passwd *passwd;

			passwd = getpwnam(user);
			if (seteuid(passwd->pw_uid) != 0)
				_exit(SETEUID_FAIL);
		}

		s = socket(sa->sa_family, type, 0);
		if (s < 0)
			_exit(SOCKET_FAIL);
		if (bind(s, sa, sa->sa_len) == 0)
			_exit(BIND_INSTANT_SUCCESS);
		if (errno != EADDRINUSE)
			_exit(BIND_BADERR1);
		if (opt != 0) {
			if (setsockopt(s, SOL_SOCKET, opt, &(int){1},
			    sizeof(int)) != 0)
				_exit(SETSOCKOPT_FAIL);
		}
		if (bind(s, sa, sa->sa_len) == 0)
			_exit(BIND_REUSE_SUCCESS);
		if (errno != EADDRINUSE)
			_exit(BIND_BADERR1);
		_exit(BIND_FAILED);
	} else {
		int status;

		ATF_REQUIRE_EQ(waitpid(child, &status, 0), child);
		ATF_REQUIRE(WIFEXITED(status));
		status = WEXITSTATUS(status);
		return (status);
	}
}

static enum bind_res
child_bind_priv(const atf_tc_t *tc, int type, struct sockaddr *sa, int opt)
{
	return (child_bind(tc, type, sa, opt, false));
}

static enum bind_res
child_bind_unpriv(const atf_tc_t *tc, int type, struct sockaddr *sa, int opt)
{
	return (child_bind(tc, type, sa, opt, true));
}

static int
bind_socket(int domain, int type, int opt, bool unspec, struct sockaddr *sa)
{
	socklen_t slen;
	int s;

	s = socket(domain, type, 0);
	ATF_REQUIRE(s >= 0);

	if (domain == AF_INET) {
		struct sockaddr_in sin;

		bzero(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		sin.sin_addr.s_addr = htonl(unspec ?
		    INADDR_ANY : INADDR_LOOPBACK);
		sin.sin_port = htons(0);
		ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);

		slen = sizeof(sin);
	} else /* if (domain == AF_INET6) */ {
		struct sockaddr_in6 sin6;

		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_addr = unspec ? in6addr_any : in6addr_loopback;
		sin6.sin6_port = htons(0);
		ATF_REQUIRE(bind(s, (struct sockaddr *)&sin6, sizeof(sin6)) == 0);

		slen = sizeof(sin6);
	}

	if (opt != 0) {
		ATF_REQUIRE(setsockopt(s, SOL_SOCKET, opt, &(int){1},
		    sizeof(int)) == 0);
	}

	ATF_REQUIRE(getsockname(s, sa, &slen) == 0);

	return (s);
}

static void
multibind_test(const atf_tc_t *tc, int domain, int type)
{
	struct sockaddr_storage ss;
	int opts[4] = { 0, SO_REUSEADDR, SO_REUSEPORT, SO_REUSEPORT_LB };
	int s;
	bool flags[2] = { false, true };
	enum bind_res res;

	for (size_t flagi = 0; flagi < nitems(flags); flagi++) {
		for (size_t opti = 0; opti < nitems(opts); opti++) {
			s = bind_socket(domain, type, opts[opti], flags[flagi],
			    (struct sockaddr *)&ss);
			for (size_t optj = 0; optj < nitems(opts); optj++) {
				int opt;

				opt = opts[optj];
				res = child_bind_priv(tc, type,
				    (struct sockaddr *)&ss, opt);
				/*
				 * Multi-binding is only allowed when both
				 * sockets have SO_REUSEPORT or SO_REUSEPORT_LB
				 * set.
				 */
				if (opts[opti] != 0 &&
				    opts[opti] != SO_REUSEADDR && opti == optj)
					ATF_REQUIRE(res == BIND_REUSE_SUCCESS);
				else
					ATF_REQUIRE(res == BIND_FAILED);

				res = child_bind_unpriv(tc, type,
				    (struct sockaddr *)&ss, opt);
				/*
				 * Multi-binding is only allowed when both
				 * sockets have the same owner.
				 */
				ATF_REQUIRE_MSG(res == BIND_FAILED,
				    "domain %u type %u opts %u:%u: "
				    "result %u (expected %u)",
				    domain, type, opts[opti], opts[optj],
				    res, BIND_FAILED);
			}
			ATF_REQUIRE(close(s) == 0);
		}
	}
}

/*
 * Try to bind two sockets to the same address/port tuple.  Under some
 * conditions this is permitted.
 */
ATF_TC(multibind);
ATF_TC_HEAD(multibind, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "unprivileged_user");
}
ATF_TC_BODY(multibind, tc)
{
	multibind_test(tc, AF_INET, SOCK_STREAM);
	multibind_test(tc, AF_INET, SOCK_DGRAM);
	multibind_test(tc, AF_INET6, SOCK_STREAM);
	multibind_test(tc, AF_INET6, SOCK_DGRAM);
}

/*
 * Test operation of bind(2) in presence of a connected inpcb using the
 * same local port.
 */
static enum bind_res
bind_connected_port_test(const atf_tc_t *tc, int domain, int type, bool wild,
    bool unpriv)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr *sinp;
	socklen_t slen;
	int error, ss, cs, as;
	enum bind_res res;

	/*
	 * Create a connected socket pair.
	 */
	ss = socket(domain, type, 0);
	ATF_REQUIRE_MSG(ss >= 0, "socket failed: %s", strerror(errno));
	if (domain == PF_INET) {
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		sin.sin_port = htons(0);
		sinp = (struct sockaddr *)&sin;
	} else {
		ATF_REQUIRE(domain == PF_INET6);
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_addr = in6addr_any;
		sin6.sin6_port = htons(0);
		sinp = (struct sockaddr *)&sin6;
	}

	error = bind(ss, sinp, sinp->sa_len);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));

	if (type == SOCK_STREAM) {
		error = getsockname(ss, sinp, &(socklen_t){ sinp->sa_len });
		ATF_REQUIRE_MSG(error == 0, "getsockname failed: %s",
		    strerror(errno));
		error = listen(ss, 1);
		ATF_REQUIRE_MSG(error == 0,
		    "listen failed: %s", strerror(errno));
		cs = socket(domain, type, 0);
		ATF_REQUIRE_MSG(cs >= 0, "socket failed: %s", strerror(errno));
		if (domain == PF_INET)
			sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		else
			sin6.sin6_addr = in6addr_loopback;
		error = connect(cs, sinp, sinp->sa_len);
		ATF_REQUIRE_MSG(error == 0,
		    "connect failed: %s", strerror(errno));
		slen = sinp->sa_len;
		as = accept(ss, sinp, &slen);
		ATF_REQUIRE_MSG(as >= 0, "accept failed: %s", strerror(errno));
	} else {
		ATF_REQUIRE(type == SOCK_DGRAM);
		if (domain == PF_INET) {
			sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			sin.sin_port = htons(6666);
		} else {
			sin6.sin6_addr = in6addr_loopback;
			sin6.sin6_port = htons(6666);
		}
		error = connect(ss, sinp, sinp->sa_len);
		ATF_REQUIRE_MSG(error == 0,
		    "connect failed: %s", strerror(errno));
		error = getsockname(ss, sinp, &(socklen_t){ sinp->sa_len });
		ATF_REQUIRE_MSG(error == 0, "getsockname failed: %s",
		    strerror(errno));
	}

	if (wild) {
		if (domain == PF_INET)
			sin.sin_addr.s_addr = htonl(INADDR_ANY);
		else
			sin6.sin6_addr = in6addr_any;
	}

	res = child_bind(tc, type, sinp, SO_REUSEADDR, unpriv);

	if (type == SOCK_STREAM) {
		ATF_REQUIRE(close(as) == 0);
		ATF_REQUIRE(close(cs) == 0);
	}
	ATF_REQUIRE(close(ss) == 0);

	return (res);
}

/*
 * Normally bind() prevents port stealing by a different user, even when
 * SO_REUSE* are specified.  However, if the port is bound by a connected
 * socket, then it's fair game.
 */
ATF_TC(bind_connected_port);
ATF_TC_HEAD(bind_connected_port, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "unprivileged_user");
}
ATF_TC_BODY(bind_connected_port, tc)
{
	struct bind_connected_port_res {
		int domain;
		int type;
		bool wild;
		bool unpriv;
		enum bind_res result;
	} tests[] = {
#define	x true
#define	o false
				     /* W  U */
	    { AF_INET,	SOCK_STREAM,	x, x, BIND_REUSE_SUCCESS },
	    { AF_INET,	SOCK_STREAM,	o, x, BIND_REUSE_SUCCESS },
	    { AF_INET,	SOCK_STREAM,	x, o, BIND_REUSE_SUCCESS },
	    { AF_INET,	SOCK_STREAM,	o, o, BIND_REUSE_SUCCESS },
	    { AF_INET6,	SOCK_STREAM,	x, x, BIND_REUSE_SUCCESS },
	    { AF_INET6,	SOCK_STREAM,	o, x, BIND_REUSE_SUCCESS },
	    { AF_INET6,	SOCK_STREAM,	x, o, BIND_REUSE_SUCCESS },
	    { AF_INET6,	SOCK_STREAM,	o, o, BIND_REUSE_SUCCESS },
	    { AF_INET,	SOCK_DGRAM,	x, x, BIND_FAILED },
	    { AF_INET,	SOCK_DGRAM,	o, x, BIND_FAILED },
	    { AF_INET,	SOCK_DGRAM,	x, o, BIND_REUSE_SUCCESS },
	    { AF_INET,	SOCK_DGRAM,	o, o, BIND_REUSE_SUCCESS },
	    { AF_INET6,	SOCK_DGRAM,	x, x, BIND_FAILED },
	    { AF_INET6,	SOCK_DGRAM,	o, x, BIND_FAILED },
	    { AF_INET6,	SOCK_DGRAM,	x, o, BIND_REUSE_SUCCESS },
	    { AF_INET6,	SOCK_DGRAM,	o, o, BIND_REUSE_SUCCESS },
#undef x
#undef o
	};

	for (u_int i = 0; i < nitems(tests); i++) {
		enum bind_res res;

		res = bind_connected_port_test(tc, tests[i].domain,
		    tests[i].type, tests[i].wild, tests[i].unpriv);
		ATF_REQUIRE_MSG(res == tests[i].result, "test #%u: "
		    "domain %u type %u%s %sprivileged: result %u (expected %u)",
		    i, tests[i].domain, tests[i].type,
		    tests[i].wild ? " wild" : "",
		    tests[i].unpriv ? "un" : "",
		    res, tests[i].result);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, basic);
	ATF_TP_ADD_TC(tp, bind_zero);
	ATF_TP_ADD_TC(tp, bind_ok);
	ATF_TP_ADD_TC(tp, poll_no_rdhup);
	ATF_TP_ADD_TC(tp, poll_rdhup);
	ATF_TP_ADD_TC(tp, stream_reconnect);
	ATF_TP_ADD_TC(tp, bindany);
	ATF_TP_ADD_TC(tp, multibind);
	ATF_TP_ADD_TC(tp, bind_connected_port);

	return atf_no_error();
}
