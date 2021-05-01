# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netgraph
BINDIR=		${TESTSDIR}

TAP_TESTS_SH+=	ng_macfilter_test

TEST_METADATA.ng_macfilter_test+=	required_user="root"
TEST_METADATA.ng_macfilter_test+=	required_programs="perl"

MAN=

.include <bsd.test.mk>
