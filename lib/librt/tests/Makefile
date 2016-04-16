# $FreeBSD$

PACKAGE=tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

LIBADD=	rt

NETBSD_ATF_TESTS_C=	sched_test
NETBSD_ATF_TESTS_C+=	sem_test

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
