# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/ttyio

# TODO: ptm_test
NETBSD_ATF_TESTS_C=	ttyio_test

DPADD.ttyio_test+=	${LIBUTIL}
LDADD.ttyio_test+=	-lutil

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
