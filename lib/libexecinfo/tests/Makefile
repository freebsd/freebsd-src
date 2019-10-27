# $FreeBSD$

PACKAGE=	tests

TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/libexecinfo

.include <bsd.own.mk>

NETBSD_ATF_TESTS_C+=	backtrace_test

LIBADD.backtrace_test+=	execinfo

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
