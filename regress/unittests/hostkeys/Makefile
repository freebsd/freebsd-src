#	$OpenBSD: Makefile,v 1.3 2016/11/01 13:43:27 tb Exp $

PROG=test_hostkeys
SRCS=tests.c test_iterate.c
REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} -d ${.CURDIR}/testdata

.include <bsd.regress.mk>
