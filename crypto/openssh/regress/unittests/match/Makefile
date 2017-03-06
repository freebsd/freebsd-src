#	$OpenBSD: Makefile,v 1.3 2016/11/01 13:43:27 tb Exp $

PROG=test_match
SRCS=tests.c
REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
