#	$OpenBSD: Makefile,v 1.1 2017/03/14 01:20:29 dtucker Exp $

PROG=test_conversion
SRCS=tests.c
REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
