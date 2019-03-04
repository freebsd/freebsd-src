# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/fs/fuse

ATF_TESTS_CXX+=	create
ATF_TESTS_CXX+=	getattr
ATF_TESTS_CXX+=	lookup
ATF_TESTS_CXX+=	open
ATF_TESTS_CXX+=	readlink
ATF_TESTS_CXX+=	setattr
ATF_TESTS_CXX+=	symlink

SRCS.create+=	create.cc
SRCS.create+=	getmntopts.c
SRCS.create+=	mockfs.cc
SRCS.create+=	utils.cc

SRCS.getattr+=	getattr.cc
SRCS.getattr+=	getmntopts.c
SRCS.getattr+=	mockfs.cc
SRCS.getattr+=	utils.cc

SRCS.lookup+=	getmntopts.c
SRCS.lookup+=	lookup.cc
SRCS.lookup+=	mockfs.cc
SRCS.lookup+=	utils.cc

SRCS.open+=	getmntopts.c
SRCS.open+=	mockfs.cc
SRCS.open+=	open.cc
SRCS.open+=	utils.cc

SRCS.readlink+=	getmntopts.c
SRCS.readlink+=	mockfs.cc
SRCS.readlink+=	readlink.cc
SRCS.readlink+=	utils.cc

SRCS.setattr+=	getmntopts.c
SRCS.setattr+=	mockfs.cc
SRCS.setattr+=	setattr.cc
SRCS.setattr+=	utils.cc

SRCS.symlink+=	getmntopts.c
SRCS.symlink+=	mockfs.cc
SRCS.symlink+=	symlink.cc
SRCS.symlink+=	utils.cc

# TODO: drastically increase timeout after test development is mostly complete
TEST_METADATA+= timeout=10

FUSEFS=		${.CURDIR:H:H:H:H}/sys/fs/fuse
MOUNT=		${.CURDIR:H:H:H:H}/sbin/mount
CFLAGS+=	-I${.CURDIR:H:H:H}
CFLAGS+=	-I${FUSEFS}
CFLAGS+=	-I${MOUNT}
.PATH:		${MOUNT}

LIBADD+=	pthread
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
