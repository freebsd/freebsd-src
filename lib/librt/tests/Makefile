# $FreeBSD$

OBJTOP=		${.OBJDIR:H:H:H}
SRCTOP=		${.CURDIR:H:H:H}
TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/librt

TESTSDIR=	${TESTSBASE}/lib/librt

LIBADD=	rt

NETBSD_ATF_TESTS_C=	sched_test
NETBSD_ATF_TESTS_C+=	sem_test

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
