# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netgraph
BINDIR=		${TESTSDIR}

ATF_TESTS_C+=	basic	\

SRCS.basic=	basic.c util.c

LIBADD+=	netgraph

.include <bsd.test.mk>
