# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

ATF_TESTS_C=	client_test
TEST_METADATA.client_test=	required_programs="devd"
TEST_METADATA.client_test+=	required_user="root"
TEST_METADATA.client_test+=	timeout=15

WARNS?=	5

.include <bsd.test.mk>
