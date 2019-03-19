# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/fs/fuse

# We could simply link all of these files into a single executable.  But since
# Kyua treats googletest programs as plain tests, it's better to separate them
# out, so we get more granular reporting.
GTESTS+=	access
GTESTS+=	create
GTESTS+=	default_permissions
GTESTS+=	destroy
GTESTS+=	flush
GTESTS+=	fsync
GTESTS+=	fsyncdir
GTESTS+=	getattr
GTESTS+=	interrupt
GTESTS+=	link
GTESTS+=	locks
GTESTS+=	lookup
GTESTS+=	mkdir
GTESTS+=	mknod
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

SRCS.access+=	access.cc
SRCS.access+=	getmntopts.c
SRCS.access+=	mockfs.cc
SRCS.access+=	utils.cc

SRCS.create+=	create.cc
SRCS.create+=	getmntopts.c
SRCS.create+=	mockfs.cc
SRCS.create+=	utils.cc

SRCS.default_permissions+=	default_permissions.cc
SRCS.default_permissions+=	getmntopts.c
SRCS.default_permissions+=	mockfs.cc
SRCS.default_permissions+=	utils.cc
TEST_METADATA.default_permissions+=	required_user="unprivileged"

SRCS.destroy+=	destroy.cc
SRCS.destroy+=	getmntopts.c
SRCS.destroy+=	mockfs.cc
SRCS.destroy+=	utils.cc

SRCS.flush+=	flush.cc
SRCS.flush+=	getmntopts.c
SRCS.flush+=	mockfs.cc
SRCS.flush+=	utils.cc

SRCS.fsync+=	fsync.cc
SRCS.fsync+=	getmntopts.c
SRCS.fsync+=	mockfs.cc
SRCS.fsync+=	utils.cc

SRCS.fsyncdir+=	fsyncdir.cc
SRCS.fsyncdir+=	getmntopts.c
SRCS.fsyncdir+=	mockfs.cc
SRCS.fsyncdir+=	utils.cc

SRCS.getattr+=	getattr.cc
SRCS.getattr+=	getmntopts.c
SRCS.getattr+=	mockfs.cc
SRCS.getattr+=	utils.cc

SRCS.interrupt+=	interrupt.cc
SRCS.interrupt+=	getmntopts.c
SRCS.interrupt+=	mockfs.cc
SRCS.interrupt+=	utils.cc

SRCS.link+=	getmntopts.c
SRCS.link+=	link.cc
SRCS.link+=	mockfs.cc
SRCS.link+=	utils.cc

SRCS.locks+=	locks.cc
SRCS.locks+=	getmntopts.c
SRCS.locks+=	mockfs.cc
SRCS.locks+=	utils.cc

SRCS.lookup+=	getmntopts.c
SRCS.lookup+=	lookup.cc
SRCS.lookup+=	mockfs.cc
SRCS.lookup+=	utils.cc

SRCS.mkdir+=	getmntopts.c
SRCS.mkdir+=	mockfs.cc
SRCS.mkdir+=	mkdir.cc
SRCS.mkdir+=	utils.cc

SRCS.mknod+=	getmntopts.c
SRCS.mknod+=	mockfs.cc
SRCS.mknod+=	mknod.cc
SRCS.mknod+=	utils.cc
TEST_METADATA.mknod+=	required_user="root"

SRCS.open+=	getmntopts.c
SRCS.open+=	mockfs.cc
SRCS.open+=	open.cc
SRCS.open+=	utils.cc

SRCS.opendir+=	getmntopts.c
SRCS.opendir+=	mockfs.cc
SRCS.opendir+=	opendir.cc
SRCS.opendir+=	utils.cc

SRCS.read+=	getmntopts.c
SRCS.read+=	mockfs.cc
SRCS.read+=	read.cc
SRCS.read+=	utils.cc

SRCS.readdir+=	getmntopts.c
SRCS.readdir+=	mockfs.cc
SRCS.readdir+=	readdir.cc
SRCS.readdir+=	utils.cc

SRCS.readlink+=	getmntopts.c
SRCS.readlink+=	mockfs.cc
SRCS.readlink+=	readlink.cc
SRCS.readlink+=	utils.cc

SRCS.release+=	getmntopts.c
SRCS.release+=	mockfs.cc
SRCS.release+=	release.cc
SRCS.release+=	utils.cc

SRCS.releasedir+=	getmntopts.c
SRCS.releasedir+=	mockfs.cc
SRCS.releasedir+=	releasedir.cc
SRCS.releasedir+=	utils.cc

SRCS.rename+=	getmntopts.c
SRCS.rename+=	mockfs.cc
SRCS.rename+=	rename.cc
SRCS.rename+=	utils.cc

SRCS.rmdir+=	getmntopts.c
SRCS.rmdir+=	mockfs.cc
SRCS.rmdir+=	rmdir.cc
SRCS.rmdir+=	utils.cc

SRCS.setattr+=	getmntopts.c
SRCS.setattr+=	mockfs.cc
SRCS.setattr+=	setattr.cc
SRCS.setattr+=	utils.cc

SRCS.statfs+=	getmntopts.c
SRCS.statfs+=	mockfs.cc
SRCS.statfs+=	statfs.cc
SRCS.statfs+=	utils.cc

SRCS.symlink+=	getmntopts.c
SRCS.symlink+=	mockfs.cc
SRCS.symlink+=	symlink.cc
SRCS.symlink+=	utils.cc

SRCS.unlink+=	getmntopts.c
SRCS.unlink+=	mockfs.cc
SRCS.unlink+=	unlink.cc
SRCS.unlink+=	utils.cc

SRCS.write+=	getmntopts.c
SRCS.write+=	mockfs.cc
SRCS.write+=	write.cc
SRCS.write+=	utils.cc

SRCS.xattr+=	getmntopts.c
SRCS.xattr+=	mockfs.cc
SRCS.xattr+=	xattr.cc
SRCS.xattr+=	utils.cc

# TODO: drastically increase timeout after test development is mostly complete
TEST_METADATA+= timeout=10

FUSEFS=		${.CURDIR:H:H:H:H}/sys/fs/fuse
MOUNT=		${.CURDIR:H:H:H:H}/sbin/mount
CFLAGS+=	-I${.CURDIR:H:H:H}
CFLAGS+=	-I${FUSEFS}
CFLAGS+=	-I${MOUNT}
.PATH:		${MOUNT}
CXXSTD=		c++14

# XXX Setting CXXFLAGS globally seems to be necessary to get mockfs.cc and
# utils.cc to build correctly.
CXXFLAGS+=	${GTESTS_CXXFLAGS}

LIBADD+=	pthread
LIBADD+=	gmock gtest
LIBADD+=	util

WARNS?=	6

.include <bsd.test.mk>
