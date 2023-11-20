
PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netgraph
BINDIR=		${TESTSDIR}

TAP_TESTS_SH+=	ng_macfilter_test

# Serialize tests since some share netgraph node names.
TEST_METADATA+=	is_exclusive=true
TEST_METADATA.ng_macfilter_test+=	required_user="root"
TEST_METADATA.ng_macfilter_test+=	required_programs="perl"

ATF_TESTS_C+=	basic		\
		bridge		\
		hub		\
		socket		\
		vlan_rotate	\

SRCS.basic=	basic.c util.c
SRCS.bridge=	bridge.c util.c
SRCS.hub=	hub.c util.c
SRCS.socket=	socket.c
SRCS.vlan_rotate=vlan_rotate.c util.c

LIBADD+=	netgraph

.include <bsd.test.mk>
