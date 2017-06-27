# $FreeBSD$

.include "${.CURDIR}/../Makefile.common"
.PATH:	${.CURDIR:H}

PLAIN_TESTS_CXX=	zfsd_unittest
SRCS.zfsd_unittest:=		${SRCS:Nzfsd_main.cc}
SRCS.zfsd_unittest+=		libmocks.c zfsd_unittest.cc
SRCS=

# Use #include <zfsd/xxx.h> in test programs.
INCFLAGS+=	-I${.CURDIR:H:H}

.if defined(DESTDIR) || defined(SYSROOT)
INCFLAGS+=		-I${SYSROOT:U${DESTDIR}}/usr/include
LDFLAGS.zfsd_unittest+=	-L${SYSROOT:U${DESTDIR}}/lib \
			-L${SYSROOT:U${DESTDIR}}/usr/lib
.endif

# Googletest options
INCFLAGS+=	-I${LOCALBASE}/include -D_THREAD_SAFE -pthread
LDFLAGS.zfsd_unittest+=	-L${LOCALBASE}/lib -D_THREAD_SAFE -pthread
LDADD.zfsd_unittest+=		${LOCALBASE}/lib/libgtest.a

# GoogleMock options
LDADD.zfsd_unittest+= ${LOCALBASE}/lib/libgmock.a ${LOCALBASE}/lib/libgmock_main.a

# Googlemock fails if we don't have this line
# https://groups.google.com/forum/#!msg/googletestframework/h8ixEPCFm0o/amwfu4xGJb0J
CFLAGS.zfsd_unittest+= -DGTEST_HAS_PTHREAD

.include <bsd.test.mk>
