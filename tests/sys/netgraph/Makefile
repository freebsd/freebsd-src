# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netgraph
BINDIR=		${TESTSDIR}

ATF_TESTS_C+=	basic		\
		bridge		\
		hub		\
		vlan_rotate	\

SRCS.basic=	basic.c util.c
SRCS.bridge=	bridge.c util.c
SRCS.hub=	hub.c util.c
SRCS.vlan_rotate=vlan_rotate.c util.c

LIBADD+=	netgraph

.include <bsd.test.mk>
