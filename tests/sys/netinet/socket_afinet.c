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

#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>

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

/*
 * Make sure that unprivileged users can't set the IP_BINDANY or IPV6_BINDANY
 * socket options.
 */
ATF_TC(socket_afinet_bindany);
ATF_TC_HEAD(socket_afinet_bindany, tc)
{
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(socket_afinet_bindany, tc)
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
 * Returns true if the bind succeeded, and false if it failed with EADDRINUSE.
 */
static bool
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
				_exit(1);
		}

		s = socket(sa->sa_family, type, 0);
		if (s < 0)
			_exit(2);
		if (bind(s, sa, sa->sa_len) == 0)
			_exit(3);
		if (errno != EADDRINUSE)
			_exit(4);
		if (opt != 0) {
			if (setsockopt(s, SOL_SOCKET, opt, &(int){1},
			    sizeof(int)) != 0)
				_exit(5);
		}
		if (bind(s, sa, sa->sa_len) == 0)
			_exit(6);
		if (errno != EADDRINUSE)
			_exit(7);
		_exit(0);
	} else {
		int status;

		ATF_REQUIRE_EQ(waitpid(child, &status, 0), child);
		ATF_REQUIRE(WIFEXITED(status));
		status = WEXITSTATUS(status);
		ATF_REQUIRE_MSG(status == 0 || status == 6,
		    "child exited with %d", status);
		return (status == 6);
	}
}

static bool
child_bind_priv(const atf_tc_t *tc, int type, struct sockaddr *sa, int opt)
{
	return (child_bind(tc, type, sa, opt, false));
}

static bool
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
	bool res;

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
					ATF_REQUIRE(res);
				else
					ATF_REQUIRE(!res);

				res = child_bind_unpriv(tc, type,
				    (struct sockaddr *)&ss, opt);
				/*
				 * Multi-binding is only allowed when both
				 * sockets have the same owner.
				 */
				ATF_REQUIRE(!res);
			}
			ATF_REQUIRE(close(s) == 0);
		}
	}
}

/*
 * Try to bind two sockets to the same address/port tuple.  Under some
 * conditions this is permitted.
 */
ATF_TC(socket_afinet_multibind);
ATF_TC_HEAD(socket_afinet_multibind, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "unprivileged_user");
}
ATF_TC_BODY(socket_afinet_multibind, tc)
{
	multibind_test(tc, AF_INET, SOCK_STREAM);
	multibind_test(tc, AF_INET, SOCK_DGRAM);
	multibind_test(tc, AF_INET6, SOCK_STREAM);
	multibind_test(tc, AF_INET6, SOCK_DGRAM);
}

static void
bind_connected_port_test(const atf_tc_t *tc, int domain)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr *sinp;
	int error, sd[3], tmp;
	bool res;

	/*
	 * Create a connected socket pair.
	 */
	sd[0] = socket(domain, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(sd[0] >= 0, "socket failed: %s", strerror(errno));
	sd[1] = socket(domain, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(sd[1] >= 0, "socket failed: %s", strerror(errno));
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

	error = bind(sd[0], sinp, sinp->sa_len);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));
	error = listen(sd[0], 1);
	ATF_REQUIRE_MSG(error == 0, "listen failed: %s", strerror(errno));

	error = getsockname(sd[0], sinp, &(socklen_t){ sinp->sa_len });
	ATF_REQUIRE_MSG(error == 0, "getsockname failed: %s", strerror(errno));
	if (domain == PF_INET)
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	error = connect(sd[1], sinp, sinp->sa_len);
	ATF_REQUIRE_MSG(error == 0, "connect failed: %s", strerror(errno));
	tmp = accept(sd[0], NULL, NULL);
	ATF_REQUIRE_MSG(tmp >= 0, "accept failed: %s", strerror(errno));
	ATF_REQUIRE(close(sd[0]) == 0);
	sd[0] = tmp;

	/* bind() should succeed even from an unprivileged user. */
	res = child_bind(tc, SOCK_STREAM, sinp, 0, false);
	ATF_REQUIRE(!res);
	res = child_bind(tc, SOCK_STREAM, sinp, 0, true);
	ATF_REQUIRE(!res);
}

/*
 * Normally bind() prevents port stealing by a different user, even when
 * SO_REUSE* are specified.  However, if the port is bound by a connected
 * socket, then it's fair game.
 */
ATF_TC(socket_afinet_bind_connected_port);
ATF_TC_HEAD(socket_afinet_bind_connected_port, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "unprivileged_user");
}
ATF_TC_BODY(socket_afinet_bind_connected_port, tc)
{
	bind_connected_port_test(tc, AF_INET);
	bind_connected_port_test(tc, AF_INET6);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, socket_afinet);
	ATF_TP_ADD_TC(tp, socket_afinet_bind_zero);
	ATF_TP_ADD_TC(tp, socket_afinet_bind_ok);
	ATF_TP_ADD_TC(tp, socket_afinet_poll_no_rdhup);
	ATF_TP_ADD_TC(tp, socket_afinet_poll_rdhup);
	ATF_TP_ADD_TC(tp, socket_afinet_stream_reconnect);
	ATF_TP_ADD_TC(tp, socket_afinet_bindany);
	ATF_TP_ADD_TC(tp, socket_afinet_multibind);
	ATF_TP_ADD_TC(tp, socket_afinet_bind_connected_port);

	return atf_no_error();
}
