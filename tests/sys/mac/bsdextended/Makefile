# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/mac/bsdextended

TAP_TESTS_C+=	ugidfw_test
TAP_TESTS_SH+=	matches_test

DPADD.ugidfw_test+=	${LIBUGIDFW}
LDADD.ugidfw_test+=	-lugidfw

TEST_METADATA.matches_test+=	required_user="root"
TEST_METADATA.ugidfw_test+=	required_user="root"

.include <bsd.test.mk>
