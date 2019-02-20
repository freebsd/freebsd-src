# $FreeBSD$

.include "${.CURDIR}/../Makefile.common"
.PATH:	${.CURDIR:H}

PLAIN_TESTS_CXX=	zfsd_unittest
SRCS.zfsd_unittest:=		${SRCS:Nzfsd_main.cc}
SRCS.zfsd_unittest+=		libmocks.c zfsd_unittest.cc
SRCS=

# Use #include <zfsd/xxx.h> in test programs.
INCFLAGS+=	-I${.CURDIR:H:H}

LIBDADD.zfsd_unittest+=	gmock_main

.include <bsd.test.mk>
