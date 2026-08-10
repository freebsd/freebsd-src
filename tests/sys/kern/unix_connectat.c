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
 * Tests for connectat(2) naming a unix-domain peer by descriptor.
 *
 * A peer can be named three ways -- as the socket object itself, as a bound
 * socket's filesystem node, or as an fdescfs /dev/fd node standing in for a
 * socket descriptor -- and each of the two filesystem nodes can be reached
 * either by an empty sun_path over a descriptor or by a pathname.  The socket
 * object has no pathname form (a path that names a descriptor is the /dev/fd
 * node, not the socket directly), giving five combinations, all of which must
 * reach the same peer:
 *
 *                          | empty sun_path (fd)      | pathname
 *   -----------------------+--------------------------+-----------------------
 *   socket object          | fd is the socket         | (n/a: a path to a
 *                          | -> stream, dgram, ...    |  descriptor is /dev/fd)
 *   -----------------------+--------------------------+-----------------------
 *   bound socket file      | O_PATH handle of the     | classic bind-path
 *   (VSOCK vnode)          | socket's vnode           | lookup
 *                          | -> empty_path_vnode      | -> path
 *   -----------------------+--------------------------+-----------------------
 *   fdescfs node of a      | O_PATH handle of the     | the "N" pathname,
 *   socket descriptor      | fdescfs node             | absolute or relative
 *   (VNON vnode)           | -> empty_path_devfd      | -> devfd,
 *                          |                          |    devfd_relative
 *
 * An empty sun_path is signalled by sun_len == offsetof(.., sun_path).
 *
 * The fdescfs cases mount their own fdescfs instance rather than relying on
 * the host's /dev/fd, so they require root; see mount_fdescfs() below.
 */

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <mntopts.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/* An AF_UNIX address with an empty path: "the fd is the peer". */
static const struct sockaddr_un empty_sun = {
	.sun_family = AF_UNIX,
	.sun_len = offsetof(struct sockaddr_un, sun_path),
};

/* Make a bound, listening stream socket. */
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

/* connectat(2) to a pathname, relative to fd (AT_FDCWD for absolute). */
static int
pathconnect(int fd, int s, const char *path)
{
	struct sockaddr_un sun = { .sun_family = AF_UNIX };

	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	sun.sun_len = SUN_LEN(&sun);
	return (connectat(fd, s, (const struct sockaddr *)&sun, sun.sun_len));
}

/* Where the fdescfs cases mount fdescfs, inside the test's work directory. */
#define	FDDIR	"fd"

/*
 * Mount an fdescfs instance on FDDIR, enabling each mount option flag in the
 * NULL-terminated 'opts' (NULL for a plain mount).  Mounting our own instance
 * rather than relying on the host's /dev/fd keeps the fdescfs cases
 * self-contained: they exercise real fdescfs lookups regardless of how the
 * host is set up, and the mode-specific behaviour below is then well defined.
 * Skips if the kernel has no fdescfs.
 */
static void
mount_fdescfs(const char * const *opts)
{
	struct iovec *iov;
	char errmsg[1024];
	int error, iovlen;

	ATF_REQUIRE_MSG(mkdir(FDDIR, 0755) == 0 || errno == EEXIST,
	    "mkdir %s: %s", FDDIR, strerror(errno));

	iov = NULL;
	iovlen = 0;
	build_iovec(&iov, &iovlen, __DECONST(char *, "fstype"),
	    __DECONST(char *, "fdescfs"), (size_t)-1);
	build_iovec(&iov, &iovlen, __DECONST(char *, "fspath"),
	    __DECONST(char *, FDDIR), (size_t)-1);
	for (; opts != NULL && *opts != NULL; opts++)
		build_iovec(&iov, &iovlen, __DECONST(char *, *opts), NULL,
		    (size_t)-1);
	build_iovec(&iov, &iovlen, __DECONST(char *, "errmsg"), errmsg,
	    sizeof(errmsg));

	errmsg[0] = '\0';
	error = nmount(iov, iovlen, 0);
	if (error != 0 && errno == ENODEV)
		atf_tc_skip("no fdescfs support in the kernel");
	ATF_REQUIRE_MSG(error == 0, "mount fdescfs on %s: %s", FDDIR,
	    errmsg[0] != '\0' ? errmsg : strerror(errno));

	free_iovec(&iov, &iovlen);
}

/* Name descriptor 'fd' within the fdescfs mounted above. */
static void
fdpath(char *buf, size_t len, int fd)
{
	int n;

	n = snprintf(buf, len, FDDIR "/%d", fd);
	ATF_REQUIRE(n > 0 && (size_t)n < len);
}

/*
 * Boilerplate for a case that mounts fdescfs: mounting requires root, and the
 * mount has to be undone even when the body fails, or the work directory
 * cannot be removed.  Each body calls mount_fdescfs() itself, choosing the
 * mount options it wants to exercise.
 */
#define	FDESCFS_TC(name)						\
	ATF_TC_WITH_CLEANUP(name);					\
	ATF_TC_HEAD(name, tc)						\
	{								\
		atf_tc_set_md_var(tc, "require.user", "root");		\
	}								\
	ATF_TC_CLEANUP(name, tc)					\
	{								\
		(void)unmount(FDDIR, 0);				\
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

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(l));
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

	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * A socket may listen while unbound, and connectat(2) reaches it by
 * descriptor: with no pathname there is nothing else that could name it.
 * mklistener() cannot be used, as it binds first.
 */
ATF_TC_WITHOUT_HEAD(listen_unbound);
ATF_TC_BODY(listen_unbound, tc)
{
	char buf[8];
	int l, s, a;

	ATF_REQUIRE((l = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_MSG(listen(l, 1) == 0, "listen: %s", strerror(errno));

	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, fdconnect(l, s));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	/* A real connection, not just an accepted descriptor. */
	ATF_REQUIRE_EQ(5, write(s, "hello", 5));
	ATF_REQUIRE_EQ(5, read(a, buf, sizeof(buf)));
	ATF_REQUIRE_EQ(0, memcmp(buf, "hello", 5));

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * A socket may be bound after it listens, so a listener can be published only
 * once it is ready to accept, rather than leaving a window in which the socket
 * file exists but connections to it are refused.  The late-bound name behaves
 * like any other.  mklistener() cannot be used: it binds first.
 */
ATF_TC_WITHOUT_HEAD(bind_after_listen);
ATF_TC_BODY(bind_after_listen, tc)
{
	struct sockaddr_un sun = { .sun_family = AF_UNIX };
	struct sockaddr_un peer;
	socklen_t len;
	int l, s, a;

	ATF_REQUIRE((l = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_MSG(listen(l, 1) == 0, "listen: %s", strerror(errno));

	strlcpy(sun.sun_path, "late.sock", sizeof(sun.sun_path));
	sun.sun_len = SUN_LEN(&sun);
	ATF_REQUIRE_MSG(bind(l, (struct sockaddr *)&sun, sun.sun_len) == 0,
	    "bind after listen: %s", strerror(errno));

	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, pathconnect(AT_FDCWD, s, "late.sock"));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	/* The name bound after listen(2) is reported to the peer. */
	memset(&peer, 0, sizeof(peer));
	len = sizeof(peer);
	ATF_REQUIRE_EQ(0, getpeername(s, (struct sockaddr *)&peer, &len));
	ATF_REQUIRE_EQ(0, strcmp(peer.sun_path, "late.sock"));

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * A socket whose connection has gone away may become a listener in its own
 * right: unp_soisdisconnected() leaves only SS_ISDISCONNECTED set, which
 * solisten_proto_check() does not reject, and unp_disconnect() has already
 * cleared unp_conn.  Only the bind requirement stood in the way, and then only
 * for the usual client socket, which has no name.
 *
 * Note: this case is here only to document the current behavior and to catch
 * it changing in the future.  Such socket reuse is not covered by the
 * specification, and is discouraged and should not be utilized in real-world
 * programs.
 */
ATF_TC_WITHOUT_HEAD(listen_after_disconnect);
ATF_TC_BODY(listen_after_disconnect, tc)
{
	int l, c, s, a;

	/* Connect a pair, then drop the accepted end to disconnect 'c'. */
	ATF_REQUIRE((l = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_MSG(listen(l, 1) == 0, "listen: %s", strerror(errno));
	ATF_REQUIRE((c = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, fdconnect(l, c));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);
	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(l));

	/* The survivor listens, and takes a connection of its own. */
	ATF_REQUIRE_MSG(listen(c, 1) == 0, "listen: %s", strerror(errno));
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, fdconnect(c, s));
	ATF_REQUIRE((a = accept(c, NULL, NULL)) >= 0);

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(c));
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

	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(p));
}

/*
 * Matrix cell: empty path + a descriptor that names a bound socket's *vnode*
 * (an O_PATH handle), not the socket object.  getsock() sees a non-socket and
 * the connect falls back to an EMPTYPATH lookup that resolves the vnode.
 */
ATF_TC_WITHOUT_HEAD(empty_path_vnode);
ATF_TC_BODY(empty_path_vnode, tc)
{
	int l, s, a, pathfd;

	l = mklistener("evnode.sock");
	ATF_REQUIRE_MSG((pathfd = open("evnode.sock", O_PATH)) >= 0,
	    "open(O_PATH): %s", strerror(errno));
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, fdconnect(pathfd, s));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(pathfd));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * Matrix cell: non-empty path naming a bound socket's vnode -- the classic
 * connect-by-pathname case, here spelled through connectat(2).
 */
ATF_TC_WITHOUT_HEAD(path);
ATF_TC_BODY(path, tc)
{
	int l, s, a;

	l = mklistener("path.sock");
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, pathconnect(AT_FDCWD, s, "path.sock"));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * Matrix cell: non-empty path that resolves to the socket *object* -- an
 * fdescfs pathname naming the listener's descriptor.  This is plain
 * connect(2), no empty path involved.
 */
FDESCFS_TC(devfd);
ATF_TC_BODY(devfd, tc)
{
	char path[32];
	int l, s, a;

	mount_fdescfs(NULL);
	l = mklistener("devfd.sock");
	fdpath(path, sizeof(path), l);

	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, pathconnect(AT_FDCWD, s, path));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * Matrix cell variant of `devfd`: the same socket-object lookup, but reached
 * through connectat(2)'s dirfd-relative resolution.  A directory descriptor
 * for the fdescfs mount serves as the base, and the peer is named by the
 * *relative* path "N" -- the listener's descriptor number.  NDINIT_ATRIGHTS
 * anchors namei() at the dirfd, and fdescfs resolves that descriptor to the
 * socket unp_connectat() connects to.
 */
FDESCFS_TC(devfd_relative);
ATF_TC_BODY(devfd_relative, tc)
{
	char path[32];
	int l, s, a, dirfd;

	mount_fdescfs(NULL);
	l = mklistener("devfd_rel.sock");
	ATF_REQUIRE_MSG((dirfd = open(FDDIR, O_DIRECTORY)) >= 0,
	    "open(%s, O_DIRECTORY): %s", FDDIR, strerror(errno));

	/* Name the listener by its fd number, relative to the fdescfs dir. */
	ATF_REQUIRE(snprintf(path, sizeof(path), "%d", l) > 0);
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, pathconnect(dirfd, s, path));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(dirfd));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * Matrix cell: empty path + an O_PATH handle to an fdescfs node.  getsock()
 * sees a non-socket, the EMPTYPATH lookup resolves the synthetic fdescfs node,
 * and opening that node yields the underlying descriptor -- the same socket.
 * Reaches the fdescfs node by descriptor rather than by pathname.
 */
FDESCFS_TC(empty_path_devfd);
ATF_TC_BODY(empty_path_devfd, tc)
{
	char path[32];
	int l, s, a, pathfd;

	mount_fdescfs(NULL);
	l = mklistener("edevfd.sock");
	fdpath(path, sizeof(path), l);
	ATF_REQUIRE_MSG((pathfd = open(path, O_PATH)) >= 0,
	    "open(%s, O_PATH): %s", path, strerror(errno));
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, fdconnect(pathfd, s));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(pathfd));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * An fdescfs pathname is resolved a single level: the descriptor it names must
 * be the peer socket itself.  A node naming an O_PATH handle instead -- of the
 * socket's *file* (VNON -> VSOCK), or of another fdescfs node (VNON -> VNON)
 * -- is not chased another level, and the connect fails with ENOTSOCK.
 *
 * The descriptor is rejected by getsock(), before the vnode behind it is ever
 * examined, so both indirections fail the same way.
 */
FDESCFS_TC(devfd_indirect);
ATF_TC_BODY(devfd_indirect, tc)
{
	char path[32], node[32];
	int l, s, pathfd, devfdfd;

	mount_fdescfs(NULL);
	l = mklistener("devfd_ind.sock");
	fdpath(node, sizeof(node), l);
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);

	/* A node naming an O_PATH handle of the socket's file. */
	ATF_REQUIRE_MSG((pathfd = open("devfd_ind.sock", O_PATH)) >= 0,
	    "open(O_PATH): %s", strerror(errno));
	fdpath(path, sizeof(path), pathfd);
	ATF_REQUIRE_ERRNO(ENOTSOCK, pathconnect(AT_FDCWD, s, path) == -1);
	ATF_REQUIRE_EQ(0, close(pathfd));

	/* A node naming an O_PATH handle of another such node. */
	ATF_REQUIRE_MSG((devfdfd = open(node, O_PATH)) >= 0,
	    "open(%s, O_PATH): %s", node, strerror(errno));
	fdpath(path, sizeof(path), devfdfd);
	ATF_REQUIRE_ERRNO(ENOTSOCK, pathconnect(AT_FDCWD, s, path) == -1);
	ATF_REQUIRE_EQ(0, close(devfdfd));

	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * The same indirection under a "nodup" mount, which is where the single-level
 * rule shows its seam: for a descriptor naming a vnode, fdescfs itself
 * dereferences to that vnode rather than presenting a synthetic node, so the
 * O_PATH handle of the socket's file resolves to the bound socket and the
 * connect succeeds.  Whether the O_PATH is followed is the mount's business;
 * resolving no more than one descriptor is ours.
 */
FDESCFS_TC(devfd_indirect_nodup);
ATF_TC_BODY(devfd_indirect_nodup, tc)
{
	static const char * const opts[] = { "nodup", NULL };
	char path[32];
	int l, s, a, pathfd;

	mount_fdescfs(opts);
	l = mklistener("devfd_nodup.sock");

	ATF_REQUIRE_MSG((pathfd = open("devfd_nodup.sock", O_PATH)) >= 0,
	    "open(O_PATH): %s", strerror(errno));
	fdpath(path, sizeof(path), pathfd);
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_EQ(0, pathconnect(AT_FDCWD, s, path));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(pathfd));
	ATF_REQUIRE_EQ(0, close(l));
}

/*
 * The mount modes differ in how fdescfs presents a descriptor node, which
 * decides whether the node can name a peer at all:
 *
 *	(plain)			VNON node, dup semantics	connects
 *	nodup			VNON node, since a socket is	connects
 *				not a vnode descriptor
 *	linrdlnk		VNON node, readlink for the	connects
 *				Linux ABI
 *	rdlnk			VLNK node, followed by namei	fails
 *
 * Only rdlnk makes the node a real symlink, and namei() then follows it;
 * fdesc_readlink() has no path to offer for a socket, so the lookup ends on
 * its "anon_inode:[unknown]" placeholder instead of the peer.  nodup composes
 * with either readlink mode without changing this: it only redirects
 * descriptors that name a vnode, which a socket descriptor does not.
 */
/*
 * Mount fdescfs with 'opts' and connect to a listener through its node.
 * 'error' is 0 if the connect must reach the peer, otherwise the errno it
 * must fail with.
 */
static void
devfd_mode(const char * const *opts, int error)
{
	char path[32];
	int l, s, a, ret;

	mount_fdescfs(opts);
	l = mklistener("mode.sock");
	fdpath(path, sizeof(path), l);
	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);

	ret = pathconnect(AT_FDCWD, s, path);
	if (error == 0) {
		ATF_REQUIRE_MSG(ret == 0, "connect: %s", strerror(errno));
		ATF_REQUIRE((a = accept(l, NULL, NULL)) >= 0);
		ATF_REQUIRE_EQ(0, close(a));
	} else {
		ATF_REQUIRE_MSG(ret == -1 && errno == error,
		    "expected %s, got %s", strerror(error),
		    ret == 0 ? "success" : strerror(errno));
	}

	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(l));
}

/* Dup semantics, the plain mount: the node names the descriptor. */
FDESCFS_TC(devfd_mode_plain);
ATF_TC_BODY(devfd_mode_plain, tc)
{
	static const char * const opts[] = { NULL };

	devfd_mode(opts, 0);
}

/* nodup only redirects descriptors that name a vnode, which a socket is not. */
FDESCFS_TC(devfd_mode_nodup);
ATF_TC_BODY(devfd_mode_nodup, tc)
{
	static const char * const opts[] = { "nodup", NULL };

	devfd_mode(opts, 0);
}

/* linrdlnk only adds readlink for the Linux ABI; the node stays VNON. */
FDESCFS_TC(devfd_mode_linrdlnk);
ATF_TC_BODY(devfd_mode_linrdlnk, tc)
{
	static const char * const opts[] = { "linrdlnk", NULL };

	devfd_mode(opts, 0);
}

FDESCFS_TC(devfd_mode_nodup_linrdlnk);
ATF_TC_BODY(devfd_mode_nodup_linrdlnk, tc)
{
	static const char * const opts[] = { "nodup", "linrdlnk", NULL };

	devfd_mode(opts, 0);
}

/*
 * rdlnk makes the node a real symlink, which namei() follows.
 * fdesc_readlink() has no path to offer for a socket, so the lookup ends on
 * its "anon_inode:[unknown]" placeholder rather than the peer.
 */
FDESCFS_TC(devfd_mode_rdlnk);
ATF_TC_BODY(devfd_mode_rdlnk, tc)
{
	static const char * const opts[] = { "rdlnk", NULL };

	devfd_mode(opts, ENOENT);
}

FDESCFS_TC(devfd_mode_nodup_rdlnk);
ATF_TC_BODY(devfd_mode_nodup_rdlnk, tc)
{
	static const char * const opts[] = { "nodup", "rdlnk", NULL };

	devfd_mode(opts, ENOENT);
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
	ATF_REQUIRE_EQ(0, close(s));
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
	ATF_REQUIRE_EQ(0, close(fd));

	/* Socket from another domain. */
	ATF_REQUIRE((fd = socket(PF_INET, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_ERRNO(EPROTOTYPE, fdconnect(fd, s) == -1);
	ATF_REQUIRE_EQ(0, close(fd));

	/* Type mismatch between the two unix sockets. */
	fd = mklistener("mismatch.sock");
	ATF_REQUIRE_ERRNO(EPROTOTYPE, fdconnect(fd, d) == -1);

	ATF_REQUIRE_EQ(0, close(fd));

	/* Stream peer that is not listening: 's' never called listen(2). */
	ATF_REQUIRE((fd = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
	ATF_REQUIRE_ERRNO(ECONNREFUSED, fdconnect(s, fd) == -1);

	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, close(d));
	ATF_REQUIRE_EQ(0, close(s));
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

	ATF_REQUIRE_EQ(0, close(a));
	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(token));
	ATF_REQUIRE_EQ(0, close(l));
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

	ATF_REQUIRE_EQ(0, close(s));
	ATF_REQUIRE_EQ(0, close(token));
	ATF_REQUIRE_EQ(0, close(l));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, stream);
	ATF_TP_ADD_TC(tp, stream_bound);
	ATF_TP_ADD_TC(tp, listen_unbound);
	ATF_TP_ADD_TC(tp, bind_after_listen);
	ATF_TP_ADD_TC(tp, listen_after_disconnect);
	ATF_TP_ADD_TC(tp, dgram);
	ATF_TP_ADD_TC(tp, empty_path_vnode);
	ATF_TP_ADD_TC(tp, path);
	ATF_TP_ADD_TC(tp, devfd);
	ATF_TP_ADD_TC(tp, devfd_relative);
	ATF_TP_ADD_TC(tp, empty_path_devfd);
	ATF_TP_ADD_TC(tp, devfd_indirect);
	ATF_TP_ADD_TC(tp, devfd_indirect_nodup);
	ATF_TP_ADD_TC(tp, devfd_mode_plain);
	ATF_TP_ADD_TC(tp, devfd_mode_nodup);
	ATF_TP_ADD_TC(tp, devfd_mode_linrdlnk);
	ATF_TP_ADD_TC(tp, devfd_mode_nodup_linrdlnk);
	ATF_TP_ADD_TC(tp, devfd_mode_rdlnk);
	ATF_TP_ADD_TC(tp, devfd_mode_nodup_rdlnk);
	ATF_TP_ADD_TC(tp, empty_path_at_fdcwd);
	ATF_TP_ADD_TC(tp, bad_peers);
	ATF_TP_ADD_TC(tp, cap_connectat);
	ATF_TP_ADD_TC(tp, cap_connectat_denied);

	return (atf_no_error());
}
