/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Ricardo Branco <rbranco@suse.de>
 * All rights reserved.
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

/* Tests for the Linux-style abstract AF_UNIX namespace. */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static socklen_t
mkabstract(struct sockaddr_un *sun, const void *name, size_t namelen)
{
	ATF_REQUIRE(namelen + 1 <= sizeof(sun->sun_path));
	memset(sun, 0, sizeof(*sun));
	sun->sun_family = AF_UNIX;
	sun->sun_path[0] = '\0';
	if (namelen > 0)
		memcpy(&sun->sun_path[1], name, namelen);
	sun->sun_len = offsetof(struct sockaddr_un, sun_path) + 1 + namelen;
	return (sun->sun_len);
}

ATF_TC_WITHOUT_HEAD(bind_and_close);
ATF_TC_BODY(bind_and_close, tc)
{
	struct sockaddr_un sun;
	socklen_t slen;
	int s;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s >= 0);
	slen = mkabstract(&sun, "test", sizeof("test") - 1);
	ATF_REQUIRE_EQ(0, bind(s, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(0, close(s));
}

ATF_TC_WITHOUT_HEAD(bind_then_unlink_does_nothing);
ATF_TC_BODY(bind_then_unlink_does_nothing, tc)
{
	struct sockaddr_un sun;
	socklen_t slen;
	int s;

	/*
	 * Abstract bindings are not in the filesystem; unlink(2) on a
	 * pathname collision should not affect them.  We just confirm
	 * binding succeeds even when the corresponding character bytes
	 * happen to look like a path.
	 */
	s = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s >= 0);
	slen = mkabstract(&sun, "/tmp/foobar", sizeof("/tmp/foobar") - 1);
	ATF_REQUIRE_EQ(0, bind(s, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(-1, access("/tmp/foobar", F_OK));
	ATF_REQUIRE_EQ(ENOENT, errno);
	close(s);
}

ATF_TC_WITHOUT_HEAD(bind_duplicate_returns_eaddrinuse);
ATF_TC_BODY(bind_duplicate_returns_eaddrinuse, tc)
{
	struct sockaddr_un sun;
	socklen_t slen;
	int s1, s2;

	s1 = socket(AF_UNIX, SOCK_STREAM, 0);
	s2 = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s1 >= 0 && s2 >= 0);
	slen = mkabstract(&sun, "dup_name", sizeof("dup_name") - 1);
	ATF_REQUIRE_EQ(0, bind(s1, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(-1, bind(s2, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(EADDRINUSE, errno);
	close(s1);
	close(s2);
}

ATF_TC_WITHOUT_HEAD(bind_after_close_succeeds);
ATF_TC_BODY(bind_after_close_succeeds, tc)
{
	struct sockaddr_un sun;
	socklen_t slen;
	int s1, s2;

	s1 = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s1 >= 0);
	slen = mkabstract(&sun, "rebind_name", sizeof("rebind_name") - 1);
	ATF_REQUIRE_EQ(0, bind(s1, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(0, close(s1));

	/*
	 * Auto-cleanup property: the binding should vanish when the last
	 * reference closes, leaving the name immediately re-bindable.
	 */
	s2 = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s2 >= 0);
	ATF_REQUIRE_EQ(0, bind(s2, (struct sockaddr *)&sun, slen));
	close(s2);
}

ATF_TC_WITHOUT_HEAD(bind_embedded_nuls_in_name);
ATF_TC_BODY(bind_embedded_nuls_in_name, tc)
{
	struct sockaddr_un sun1, sun2;
	socklen_t slen1, slen2;
	int s1, s2;

	/*
	 * Names with embedded NULs are distinct byte sequences: "a\0b" and
	 * "a\0c" must not collide.  This exercises the "do not use string
	 * functions on abstract names" invariant.
	 */
	s1 = socket(AF_UNIX, SOCK_STREAM, 0);
	s2 = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s1 >= 0 && s2 >= 0);
	slen1 = mkabstract(&sun1, "a\0b", sizeof("a\0b") - 1);
	slen2 = mkabstract(&sun2, "a\0c", sizeof("a\0c") - 1);
	ATF_REQUIRE_EQ(0, bind(s1, (struct sockaddr *)&sun1, slen1));
	ATF_REQUIRE_EQ(0, bind(s2, (struct sockaddr *)&sun2, slen2));
	close(s1);
	close(s2);
}

ATF_TC_WITHOUT_HEAD(bind_filesystem_name_does_not_collide);
ATF_TC_BODY(bind_filesystem_name_does_not_collide, tc)
{
	struct sockaddr_un fs_sun, abs_sun;
	socklen_t abs_slen;
	int sf, sa;

	/*
	 * Pathname and abstract namespaces are disjoint: binding "foo" in
	 * the filesystem must not prevent binding "\0foo" in abstract.
	 */
	memset(&fs_sun, 0, sizeof(fs_sun));
	fs_sun.sun_family = AF_UNIX;
	snprintf(fs_sun.sun_path, sizeof(fs_sun.sun_path),
	    "test_fs_collide_%u.sock", (unsigned)getpid());
	fs_sun.sun_len = SUN_LEN(&fs_sun);
	(void)unlink(fs_sun.sun_path);

	sf = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(sf >= 0);
	ATF_REQUIRE_EQ(0, bind(sf, (struct sockaddr *)&fs_sun, fs_sun.sun_len));

	sa = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(sa >= 0);
	abs_slen = mkabstract(&abs_sun, fs_sun.sun_path,
	    strlen(fs_sun.sun_path));
	ATF_REQUIRE_EQ(0, bind(sa, (struct sockaddr *)&abs_sun, abs_slen));

	close(sa);
	close(sf);
	unlink(fs_sun.sun_path);
}

ATF_TC_WITHOUT_HEAD(getsockname_roundtrip);
ATF_TC_BODY(getsockname_roundtrip, tc)
{
	struct sockaddr_un bound, got;
	socklen_t blen, glen;
	int s;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s >= 0);
	blen = mkabstract(&bound, "rtrip", sizeof("rtrip") - 1);
	ATF_REQUIRE_EQ(0, bind(s, (struct sockaddr *)&bound, blen));

	glen = sizeof(got);
	memset(&got, 0xa5, sizeof(got));
	ATF_REQUIRE_EQ(0, getsockname(s, (struct sockaddr *)&got, &glen));
	ATF_REQUIRE_EQ(blen, glen);
	ATF_REQUIRE_EQ(AF_UNIX, got.sun_family);
	ATF_REQUIRE_EQ(0, got.sun_path[0]);
	ATF_REQUIRE_EQ(0, memcmp(got.sun_path, bound.sun_path,
	    blen - offsetof(struct sockaddr_un, sun_path)));
	close(s);
}

ATF_TC_WITHOUT_HEAD(stream_connect_econnrefused_on_missing);
ATF_TC_BODY(stream_connect_econnrefused_on_missing, tc)
{
	struct sockaddr_un sun;
	socklen_t slen;
	int s;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s >= 0);
	slen = mkabstract(&sun, "not_bound", sizeof("not_bound") - 1);
	ATF_REQUIRE_EQ(-1, connect(s, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(ECONNREFUSED, errno);
	close(s);
}

ATF_TC_WITHOUT_HEAD(stream_connect_accept);
ATF_TC_BODY(stream_connect_accept, tc)
{
	struct sockaddr_un sun;
	char buf[5];
	socklen_t slen;
	int ls, cs, as;

	ls = socket(AF_UNIX, SOCK_STREAM, 0);
	cs = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(ls >= 0 && cs >= 0);
	slen = mkabstract(&sun, "stream_listen", sizeof("stream_listen") - 1);
	ATF_REQUIRE_EQ(0, bind(ls, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(0, listen(ls, 1));
	ATF_REQUIRE_EQ(0, connect(cs, (struct sockaddr *)&sun, slen));
	as = accept(ls, NULL, NULL);
	ATF_REQUIRE(as >= 0);
	ATF_REQUIRE_EQ(5, write(cs, "hello", 5));
	ATF_REQUIRE_EQ(5, read(as, buf, 5));
	ATF_REQUIRE_EQ(0, memcmp(buf, "hello", 5));
	close(as);
	close(cs);
	close(ls);
}

ATF_TC_WITHOUT_HEAD(seqpacket_connect_accept);
ATF_TC_BODY(seqpacket_connect_accept, tc)
{
	struct sockaddr_un sun;
	char buf[16];
	socklen_t slen;
	int ls, cs, as;
	ssize_t n;

	ls = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	cs = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(ls >= 0 && cs >= 0);
	slen = mkabstract(&sun, "seqpacket", sizeof("seqpacket") - 1);
	ATF_REQUIRE_EQ(0, bind(ls, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(0, listen(ls, 1));
	ATF_REQUIRE_EQ(0, connect(cs, (struct sockaddr *)&sun, slen));
	as = accept(ls, NULL, NULL);
	ATF_REQUIRE(as >= 0);
	ATF_REQUIRE_EQ(7, write(cs, "datagram", 7));
	n = read(as, buf, sizeof(buf));
	ATF_REQUIRE_EQ(7, n);
	close(as);
	close(cs);
	close(ls);
}

ATF_TC_WITHOUT_HEAD(dgram_connect_send);
ATF_TC_BODY(dgram_connect_send, tc)
{
	struct sockaddr_un sun;
	char buf[8];
	socklen_t slen;
	int rs, cs;

	rs = socket(AF_UNIX, SOCK_DGRAM, 0);
	cs = socket(AF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(rs >= 0 && cs >= 0);
	slen = mkabstract(&sun, "dgram_recv", sizeof("dgram_recv") - 1);
	ATF_REQUIRE_EQ(0, bind(rs, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(0, connect(cs, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(4, send(cs, "ping", 4, 0));
	ATF_REQUIRE_EQ(4, recv(rs, buf, sizeof(buf), 0));
	ATF_REQUIRE_EQ(0, memcmp(buf, "ping", 4));
	close(cs);
	close(rs);
}

ATF_TC_WITHOUT_HEAD(dgram_unconnected_sendto);
ATF_TC_BODY(dgram_unconnected_sendto, tc)
{
	struct sockaddr_un sun;
	char buf[8];
	socklen_t slen;
	int rs, cs;

	rs = socket(AF_UNIX, SOCK_DGRAM, 0);
	cs = socket(AF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(rs >= 0 && cs >= 0);
	slen = mkabstract(&sun, "dgram_unconn", sizeof("dgram_unconn") - 1);
	ATF_REQUIRE_EQ(0, bind(rs, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(4, sendto(cs, "ping", 4, 0,
	    (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(4, recv(rs, buf, sizeof(buf), 0));
	ATF_REQUIRE_EQ(0, memcmp(buf, "ping", 4));
	close(cs);
	close(rs);
}

ATF_TC_WITHOUT_HEAD(dgram_unconnected_sendto_loop);
ATF_TC_BODY(dgram_unconnected_sendto_loop, tc)
{
	struct sockaddr_un sun;
	char buf[8];
	socklen_t slen;
	int rs, cs;

	rs = socket(AF_UNIX, SOCK_DGRAM, 0);
	cs = socket(AF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(rs >= 0 && cs >= 0);
	slen = mkabstract(&sun, "dgram_loop", sizeof("dgram_loop") - 1);
	ATF_REQUIRE_EQ(0, bind(rs, (struct sockaddr *)&sun, slen));
	for (int i = 0; i < 4096; i++) {
		ATF_REQUIRE_EQ(4, sendto(cs, "ping", 4, 0,
		    (struct sockaddr *)&sun, slen));
		ATF_REQUIRE_EQ(4, recv(rs, buf, sizeof(buf), 0));
	}
	close(cs);
	close(rs);
}

ATF_TC_WITHOUT_HEAD(stream_peer_address);
ATF_TC_BODY(stream_peer_address, tc)
{
	struct sockaddr_un bound, peer;
	socklen_t blen, plen;
	int ls, cs, as;

	ls = socket(AF_UNIX, SOCK_STREAM, 0);
	cs = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(ls >= 0 && cs >= 0);
	blen = mkabstract(&bound, "peer_addr", sizeof("peer_addr") - 1);
	ATF_REQUIRE_EQ(0, bind(ls, (struct sockaddr *)&bound, blen));
	ATF_REQUIRE_EQ(0, listen(ls, 1));
	ATF_REQUIRE_EQ(0, connect(cs, (struct sockaddr *)&bound, blen));
	as = accept(ls, NULL, NULL);
	ATF_REQUIRE(as >= 0);

	plen = sizeof(peer);
	memset(&peer, 0, sizeof(peer));
	ATF_REQUIRE_EQ(0, getpeername(cs, (struct sockaddr *)&peer, &plen));
	ATF_REQUIRE_EQ(blen, plen);
	ATF_REQUIRE_EQ(0, peer.sun_path[0]);
	ATF_REQUIRE_EQ(0, memcmp(peer.sun_path, bound.sun_path,
	    blen - offsetof(struct sockaddr_un, sun_path)));

	close(as);
	close(cs);
	close(ls);
}

/*
 * Auto-cleanup: a listener that exits without closing should free the
 * binding when the kernel reaps the fd.
 */
ATF_TC_WITHOUT_HEAD(child_exit_releases_binding);
ATF_TC_BODY(child_exit_releases_binding, tc)
{
	struct sockaddr_un sun;
	socklen_t slen;
	pid_t pid;
	int s, status;

	slen = mkabstract(&sun, "child_releases", sizeof("child_releases") - 1);

	pid = fork();
	ATF_REQUIRE(pid >= 0);
	if (pid == 0) {
		s = socket(AF_UNIX, SOCK_STREAM, 0);
		if (s < 0)
			_exit(1);
		if (bind(s, (struct sockaddr *)&sun, slen) != 0)
			_exit(2);
		_exit(0);	/* fd reaped on exit; binding should vanish */
	}
	ATF_REQUIRE_EQ(pid, waitpid(pid, &status, 0));
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(0, WEXITSTATUS(status));

	/* Parent should now be able to bind the same name. */
	s = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s >= 0);
	ATF_REQUIRE_EQ(0, bind(s, (struct sockaddr *)&sun, slen));
	close(s);
}

/*
 * The same abstract name may be bound simultaneously by sockets of
 * different so_type within one prison.  Linux behaves this way (the
 * abstract namespace hash key includes sk_type), so Linuxulator
 * compatibility requires we match it.  Same name, same type still
 * collides with EADDRINUSE.
 */
ATF_TC_WITHOUT_HEAD(type_independence);
ATF_TC_BODY(type_independence, tc)
{
	struct sockaddr_un sun;
	socklen_t slen;
	int s_stream, s_dgram, s_seq, s_stream2;

	slen = mkabstract(&sun, "type_indep", sizeof("type_indep") - 1);

	s_stream = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s_stream >= 0);
	ATF_REQUIRE_EQ(0, bind(s_stream, (struct sockaddr *)&sun, slen));

	s_dgram = socket(AF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(s_dgram >= 0);
	ATF_CHECK_EQ_MSG(0, bind(s_dgram, (struct sockaddr *)&sun, slen),
	    "SOCK_DGRAM bind to same name as SOCK_STREAM should succeed: %s",
	    strerror(errno));

	s_seq = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s_seq >= 0);
	ATF_CHECK_EQ_MSG(0, bind(s_seq, (struct sockaddr *)&sun, slen),
	    "SOCK_SEQPACKET bind to same name should succeed: %s",
	    strerror(errno));

	/* Same name, same type: must still collide. */
	s_stream2 = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s_stream2 >= 0);
	ATF_REQUIRE_EQ(-1, bind(s_stream2, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(EADDRINUSE, errno);

	close(s_stream);
	close(s_dgram);
	close(s_seq);
	close(s_stream2);
}

ATF_TC_WITHOUT_HEAD(chmod_rejected);
ATF_TC_BODY(chmod_rejected, tc)
{
	struct sockaddr_un sun;
	socklen_t slen;
	int s;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	ATF_REQUIRE(s >= 0);
	slen = mkabstract(&sun, "chmod_test", sizeof("chmod_test") - 1);
	ATF_REQUIRE_EQ(0, bind(s, (struct sockaddr *)&sun, slen));
	ATF_REQUIRE_EQ(-1, fchmod(s, 0600));
	ATF_REQUIRE_EQ(EINVAL, errno);
	close(s);
}

/*
 * Autobind: bind(2) with a length-1 abstract address (just the NUL marker,
 * no following bytes) triggers kernel-assigned name selection.  The kernel
 * picks a unique \0NNNNN name (NUL + 5 lowercase hex digits), binds the
 * socket to it, and getsockname(2) returns the assigned address.  Two
 * autobinds within the same prison and socket type must produce distinct
 * names.  The assigned address is usable for datagram exchange.
 */
ATF_TC_WITHOUT_HEAD(autobind);
ATF_TC_BODY(autobind, tc)
{
	struct sockaddr_un trigger, bound, bound2;
	socklen_t blen;
	int s1, s2, cs, i;
	char buf[1];

	/*
	 * Autobind trigger: sun_path[0] == '\0' and namelen == 1.
	 * sun_len = offsetof(sun_path) + 1.
	 */
	memset(&trigger, 0, sizeof(trigger));
	trigger.sun_family = AF_UNIX;
	trigger.sun_len = offsetof(struct sockaddr_un, sun_path) + 1;

	s1 = socket(AF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(s1 >= 0);
	ATF_REQUIRE_EQ(0, bind(s1, (struct sockaddr *)&trigger,
	    trigger.sun_len));

	/* getsockname must return a 6-byte abstract name: \0 + 5 hex digits. */
	blen = sizeof(bound);
	ATF_REQUIRE_EQ(0, getsockname(s1, (struct sockaddr *)&bound, &blen));
	ATF_REQUIRE_EQ((socklen_t)(offsetof(struct sockaddr_un, sun_path) + 6),
	    blen);
	ATF_REQUIRE_EQ('\0', bound.sun_path[0]);
	for (i = 1; i <= 5; i++)
		ATF_REQUIRE_MSG(isxdigit((unsigned char)bound.sun_path[i]),
		    "sun_path[%d] = 0x%02x is not a hex digit", i,
		    (unsigned char)bound.sun_path[i]);

	/* Second autobind must produce a distinct name (same type, same prison). */
	s2 = socket(AF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(s2 >= 0);
	ATF_REQUIRE_EQ(0, bind(s2, (struct sockaddr *)&trigger,
	    trigger.sun_len));
	blen = sizeof(bound2);
	ATF_REQUIRE_EQ(0, getsockname(s2, (struct sockaddr *)&bound2, &blen));
	ATF_REQUIRE_MSG(
	    memcmp(bound.sun_path, bound2.sun_path, 6) != 0,
	    "two autobinds produced the same name");

	/* The assigned address is reachable: another socket can send to it. */
	blen = offsetof(struct sockaddr_un, sun_path) + 6;
	cs = socket(AF_UNIX, SOCK_DGRAM, 0);
	ATF_REQUIRE(cs >= 0);
	ATF_REQUIRE_EQ(1, sendto(cs, "x", 1, 0,
	    (struct sockaddr *)&bound, blen));
	ATF_REQUIRE_EQ(1, recv(s1, buf, sizeof(buf), 0));
	ATF_REQUIRE_EQ('x', buf[0]);

	close(cs);
	close(s1);
	close(s2);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bind_and_close);
	ATF_TP_ADD_TC(tp, bind_then_unlink_does_nothing);
	ATF_TP_ADD_TC(tp, bind_duplicate_returns_eaddrinuse);
	ATF_TP_ADD_TC(tp, bind_after_close_succeeds);
	ATF_TP_ADD_TC(tp, bind_embedded_nuls_in_name);
	ATF_TP_ADD_TC(tp, bind_filesystem_name_does_not_collide);
	ATF_TP_ADD_TC(tp, getsockname_roundtrip);
	ATF_TP_ADD_TC(tp, stream_connect_econnrefused_on_missing);
	ATF_TP_ADD_TC(tp, stream_connect_accept);
	ATF_TP_ADD_TC(tp, seqpacket_connect_accept);
	ATF_TP_ADD_TC(tp, dgram_connect_send);
	ATF_TP_ADD_TC(tp, dgram_unconnected_sendto);
	ATF_TP_ADD_TC(tp, dgram_unconnected_sendto_loop);
	ATF_TP_ADD_TC(tp, stream_peer_address);
	ATF_TP_ADD_TC(tp, child_exit_releases_binding);
	ATF_TP_ADD_TC(tp, type_independence);
	ATF_TP_ADD_TC(tp, chmod_rejected);
	ATF_TP_ADD_TC(tp, autobind);
	return (atf_no_error());
}
