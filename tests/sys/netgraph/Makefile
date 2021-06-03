# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netgraph
BINDIR=		${TESTSDIR}

ATF_TESTS_C+=	basic	\
		bridge	\
		hub	\

SRCS.basic=	basic.c util.c
SRCS.bridge=	bridge.c util.c
SRCS.hub=	hub.c util.c

LIBADD+=	netgraph

.include <bsd.test.mk>
