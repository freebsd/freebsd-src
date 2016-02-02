# $FreeBSD$

PACKAGE=tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

ATF_TESTS_C+= crypt_tests

NETBSD_ATF_TESTS_C+= crypt_test

CFLAGS+= -I${.CURDIR:H}
LIBADD=	crypt

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
