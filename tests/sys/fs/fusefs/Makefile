# $FreeBSD$

.include <bsd.compiler.mk>

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/fs/fusefs

# We could simply link all of these files into a single executable.  But since
# Kyua treats googletest programs as plain tests, it's better to separate them
# out, so we get more granular reporting.
GTESTS+=	access
GTESTS+=	allow_other
GTESTS+=	bmap
GTESTS+=	create
GTESTS+=	default_permissions
GTESTS+=	default_permissions_privileged
GTESTS+=	destroy
GTESTS+=	dev_fuse_poll
GTESTS+=	fifo
GTESTS+=	flush
GTESTS+=	forget
GTESTS+=	fsync
GTESTS+=	fsyncdir
GTESTS+=	getattr
GTESTS+=	interrupt
GTESTS+=	io
GTESTS+=	link
GTESTS+=	locks
GTESTS+=	lookup
GTESTS+=	mkdir
GTESTS+=	mknod
GTESTS+=	mount
GTESTS+=	nfs
GTESTS+=	notify
GTESTS+=	open
GTESTS+=	opendir
GTESTS+=	read
GTESTS+=	readdir
GTESTS+=	readlink
GTESTS+=	release
GTESTS+=	releasedir
GTESTS+=	rename
GTESTS+=	rmdir
GTESTS+=	setattr
GTESTS+=	statfs
GTESTS+=	symlink
GTESTS+=	unlink
GTESTS+=	write
GTESTS+=	xattr

.for p in ${GTESTS}
SRCS.$p+=	${p}.cc
SRCS.$p+=	getmntopts.c
SRCS.$p+=	mockfs.cc
SRCS.$p+=	utils.cc
.endfor

TEST_METADATA.default_permissions+=	required_user="unprivileged"
TEST_METADATA.default_permissions_privileged+=	required_user="root"
TEST_METADATA.mknod+=	required_user="root"
TEST_METADATA.nfs+=	required_user="root"

# TODO: drastically increase timeout after test development is mostly complete
TEST_METADATA+= timeout=10

FUSEFS=		${SRCTOP}/sys/fs/fuse
MOUNT=		${SRCTOP}/sbin/mount
# Suppress warnings that GCC generates for the libc++ and gtest headers.
CXXWARNFLAGS.gcc+=	-Wno-placement-new -Wno-attributes
.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 80000
CXXWARNFLAGS+=	-Wno-class-memaccess
.endif
CXXFLAGS+=	-I${SRCTOP}/tests
CXXFLAGS+=	-I${FUSEFS}
CXXFLAGS+=	-I${MOUNT}
.PATH:		${MOUNT}
CXXSTD=		c++14

LIBADD+=	pthread
LIBADD+=	gmock gtest
LIBADD+=	util

WARNS?=	6

.include <bsd.test.mk>
