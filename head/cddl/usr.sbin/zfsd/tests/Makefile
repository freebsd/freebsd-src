# $FreeBSD$

SRCDIR=${.CURDIR}/../../../..
.include "${.CURDIR}/../Makefile.common"
.PATH:	${.CURDIR}/..

PROG_CXX=	zfsd_unittest
SRCS:=		${SRCS:Nzfsd_main.cc}
SRCS+=		libmocks.c zfsd_unittest.cc

# Use #include <zfsd/xxx.h> in test programs.
INCFLAGS+=	-I ${.CURDIR}/../..

.if defined(DESTDIR)
INCFLAGS+=	-I ${DESTDIR}/usr/include
LIBRARY_PATH=	${DESTDIR}/lib:${DESTDIR}/usr/lib
LDFLAGS+=	-L ${DESTDIR}/lib -L ${DESTDIR}/usr/lib
.elif defined(WORLDTMP)
INCFLAGS+=	-I ${WORLDTMP}/usr/include
LIBRARY_PATH=	${WORLDTMP}/lib:${WORLDTMP}/usr/lib
LDFLAGS+=	-L ${WORLDTMP}/lib -L ${WORLDTMP}/usr/lib
.else
LIBRARY_PATH=
.endif
ZFSD_UNITTEST=	env LD_LIBRARY_PATH=${LIBRARY_PATH} ./zfsd_unittest

# Googletest options
LOCALBASE?=	/usr/local
INCFLAGS+=	-I ${LOCALBASE}/include -D_THREAD_SAFE -pthread
LDFLAGS+=	-L ${LOCALBASE}/lib -D_THREAD_SAFE -pthread
LDADD+=		${LOCALBASE}/lib/libgtest.a

# GoogleMock options
LDADD+= ${LOCALBASE}/lib/libgmock.a ${LOCALBASE}/lib/libgmock_main.a

# Googlemock fails if we don't have this line
# https://groups.google.com/forum/#!msg/googletestframework/h8ixEPCFm0o/amwfu4xGJb0J
CFLAGS+= -DGTEST_HAS_PTHREAD

# Install the tests
TESTSBASE?=	/usr/tests
TESTSDIR?=	${TESTSBASE}/cddl/sbin/zfsd
# TODO: Convert from an ATF SH test to a Kyua plain test
# Long term TODO: Convert to a Kyua googletest test
ATF_TESTS_SH+=	zfsd_test
BINDIR=		${TESTSDIR}

.include <bsd.test.mk>
