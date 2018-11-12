#	$OpenBSD: Makefile,v 1.2 2015/01/24 10:39:21 miod Exp $

TEST_ENV=      "MALLOC_OPTIONS=AFGJPRX"

PROG=test_kex
SRCS=tests.c test_kex.c
REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>

LDADD+=-lz
