#	$OpenBSD: Makefile,v 1.4 2016/11/01 13:43:27 tb Exp $

PROG=test_utf8
SRCS=tests.c
REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
