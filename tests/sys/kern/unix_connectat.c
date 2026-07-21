/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 John Ericson
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Tests for connectat(2) where the descriptor argument names the peer
 * unix socket directly, signalled by an empty sun_path.
 */

#include <sys/socket.h>
#include <sys/capsicum.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/* An AF_UNIX address with an empty path: "the fd is the peer". */
static const struct sockaddr_un empty_sun = {
	.sun_family = AF_UNIX,
	.sun_len = offsetof(struct sockaddr_un, sun_path),
};

/*
 * Make a bound, listening stream socket.  Binding is not optional:
 * uipc_listen() refuses unbound sockets with EDESTADDRREQ.
 */
static int
mklistener(const char *path)
{
	struct sockaddr_un sun = { .sun_family = AF_UNIX };
	int l;

	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	sun.sun_len = SUN_LEN(&sun);
	ATF_REQUIRE((l = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_MSG(bind(l, (struct sockaddr *)&sun, sun.sun_len) == 0,
	    "bind(%s): %s", path, strerror(errno));
	ATF_REQUIRE_MSG(listen(l, 1) == 0, "listen: %s", strerror(errno));
	return (l);
}

static int
fdconnect(int fd, int s)
{
	return (connectat(fd, s, (const struct sockaddr *)&empty_sun,
	    empty_sun.sun_len));
}

/* Connect to a listening stream socket by its fd; pass data. */
ATF_TC_WITHOUT_HEAD(stream);
ATF_TC_BODY(stream, tc)
{
	char buf[8];
	int l, s, a;

	l = mklistener("stream.sock");
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, fdconnect(l, s));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	ATF_REQUIRE_EQ(5, write(s, "hello", 5));
	ATF_REQUIRE_EQ(5, read(a, buf, sizeof(buf)));
	ATF_REQUIRE_EQ(0, memcmp(buf, "hello", 5));
	ATF_REQUIRE_EQ(5, write(a, "world", 5));
	ATF_REQUIRE_EQ(5, read(s, buf, sizeof(buf)));
	ATF_REQUIRE_EQ(0, memcmp(buf, "world", 5));

	close(a);
	close(s);
	close(l);
}

/* A bound listener's path is still reported to the connecting side. */
ATF_TC_WITHOUT_HEAD(stream_bound);
ATF_TC_BODY(stream_bound, tc)
{
	struct sockaddr_un sun;
	socklen_t len;
	int l, s;

	l = mklistener("bound.sock");
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, fdconnect(l, s));

	memset(&sun, 0, sizeof(sun));
	len = sizeof(sun);
	ATF_REQUIRE_EQ(0, getpeername(s, (struct sockaddr *)&sun, &len));
	ATF_REQUIRE_EQ(0, strcmp(sun.sun_path, "bound.sock"));

	close(s);
	close(l);
}

/* Connect a datagram socket to an unbound peer by its fd. */
ATF_TC_WITHOUT_HEAD(dgram);
ATF_TC_BODY(dgram, tc)
{
	char buf[8];
	int p, s;

	ATF_REQUIRE((p = socket(PF_UNIX, SOCK_DGRAM, 0)) >= 0);
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_DGRAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, fdconnect(p, s));
	ATF_REQUIRE_EQ(5, send(s, "hello", 5, 0));
	ATF_REQUIRE_EQ(5, recv(p, buf, sizeof(buf), 0));
	ATF_REQUIRE_EQ(0, memcmp(buf, "hello", 5));

	close(s);
	close(p);
}

/* An empty path is only meaningful with a real descriptor. */
ATF_TC_WITHOUT_HEAD(empty_path_at_fdcwd);
ATF_TC_BODY(empty_path_at_fdcwd, tc)
{
	int s;

	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_ERRNO(EINVAL, connect(s,
	    (const struct sockaddr *)&empty_sun, empty_sun.sun_len) == -1);
	ATF_REQUIRE_ERRNO(EINVAL, fdconnect(AT_FDCWD, s) == -1);
	close(s);
}

/* Error matrix for unsuitable descriptors and peers. */
ATF_TC_WITHOUT_HEAD(bad_peers);
ATF_TC_BODY(bad_peers, tc)
{
	int s, d, fd;

	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE((d = socket(PF_UNIX, SOCK_DGRAM, 0)) >= 0);

	/* Non-socket descriptor. */
	ATF_REQUIRE((fd = open(".", O_RDONLY)) >= 0);
	ATF_REQUIRE_ERRNO(ENOTSOCK, fdconnect(fd, s) == -1);
	close(fd);

	/* Socket from another domain. */
	ATF_REQUIRE((fd = socket(PF_INET, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_ERRNO(EPROTOTYPE, fdconnect(fd, s) == -1);
	close(fd);

	/* Type mismatch between the two unix sockets. */
	fd = mklistener("mismatch.sock");
	ATF_REQUIRE_ERRNO(EPROTOTYPE, fdconnect(fd, d) == -1);

	close(fd);

	/* Stream peer that is not listening: 's' never called listen(2). */
	ATF_REQUIRE((fd = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_ERRNO(ECONNREFUSED, fdconnect(s, fd) == -1);

	close(fd);
	close(d);
	close(s);
}

/*
 * A descriptor limited to CAP_CONNECTAT is a pure connect-to-me token:
 * it can be connected to, but not listened on, accepted from, or read.
 */
ATF_TC_WITHOUT_HEAD(cap_connectat);
ATF_TC_BODY(cap_connectat, tc)
{
	cap_rights_t rights;
	char buf[8];
	int l, s, token, a;

	l = mklistener("cap.sock");
	ATF_REQUIRE((token = dup(l)) >= 0);
	ATF_REQUIRE_EQ(0, cap_rights_limit(token,
	    cap_rights_init(&rights, CAP_CONNECTAT)));

	ATF_REQUIRE_ERRNO(ENOTCAPABLE, listen(token, 1) == -1);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, accept(token, NULL, NULL) == -1);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, read(token, buf, sizeof(buf)) == -1);

	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, fdconnect(token, s));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	close(a);
	close(s);
	close(token);
	close(l);
}

/* Without CAP_CONNECTAT, the descriptor cannot be a connect target. */
ATF_TC_WITHOUT_HEAD(cap_connectat_denied);
ATF_TC_BODY(cap_connectat_denied, tc)
{
	cap_rights_t rights;
	int l, s, token;

	l = mklistener("capdeny.sock");
	ATF_REQUIRE((token = dup(l)) >= 0);
	ATF_REQUIRE_EQ(0, cap_rights_limit(token,
	    cap_rights_init(&rights, CAP_READ, CAP_WRITE)));

	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, fdconnect(token, s) == -1);

	close(s);
	close(token);
	close(l);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, stream);
	ATF_TP_ADD_TC(tp, stream_bound);
	ATF_TP_ADD_TC(tp, dgram);
	ATF_TP_ADD_TC(tp, empty_path_at_fdcwd);
	ATF_TP_ADD_TC(tp, bad_peers);
	ATF_TP_ADD_TC(tp, cap_connectat);
	ATF_TP_ADD_TC(tp, cap_connectat_denied);

	return (atf_no_error());
}
