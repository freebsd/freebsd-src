# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/fs/fuse

# We could simply link all of these files into a single executable.  But since
# Kyua treats googletest programs as plain tests, it's better to separate them
# out, so we get more granular reporting.
ATF_TESTS_CXX+=	access
ATF_TESTS_CXX+=	create
ATF_TESTS_CXX+=	default_permissions
ATF_TESTS_CXX+=	destroy
ATF_TESTS_CXX+=	flush
ATF_TESTS_CXX+=	fsync
ATF_TESTS_CXX+=	fsyncdir
ATF_TESTS_CXX+=	getattr
ATF_TESTS_CXX+=	interrupt
ATF_TESTS_CXX+=	link
ATF_TESTS_CXX+=	locks
ATF_TESTS_CXX+=	lookup
ATF_TESTS_CXX+=	mkdir
ATF_TESTS_CXX+=	mknod
ATF_TESTS_CXX+=	open
ATF_TESTS_CXX+=	opendir
ATF_TESTS_CXX+=	read
ATF_TESTS_CXX+=	readdir
ATF_TESTS_CXX+=	readlink
ATF_TESTS_CXX+=	release
ATF_TESTS_CXX+=	releasedir
ATF_TESTS_CXX+=	rename
ATF_TESTS_CXX+=	rmdir
ATF_TESTS_CXX+=	setattr
ATF_TESTS_CXX+=	statfs
ATF_TESTS_CXX+=	symlink
ATF_TESTS_CXX+=	unlink
ATF_TESTS_CXX+=	write
ATF_TESTS_CXX+=	xattr

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

LIBADD+=	util pthread
WARNS?=	6
NO_WTHREAD_SAFETY=	# GoogleTest fails Clang's thread safety check

# Use googlemock from ports until after the import-googletest-1.8.1 branch
# merges to head.
CXXFLAGS+=	-I/usr/local/include
CXXFLAGS+=	-DGTEST_HAS_POSIX_RE=1
CXXFLAGS+=	-DGTEST_HAS_PTHREAD=1
CXXFLAGS+=	-DGTEST_HAS_STREAM_REDIRECTION=1
CXXFLAGS+=	-frtti
CXXFLAGS+=	-std=c++14
LDADD+=		${LOCALBASE}/lib/libgmock.a
LDADD+=		${LOCALBASE}/lib/libgtest.a
# Without -lpthread, gtest fails at _runtime_ with the error pthread_key_create(&key, &DeleteThreadLocalValue)failed with error 78
LIBADD+=	pthread

.include <bsd.test.mk>
