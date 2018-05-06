#	$OpenBSD: Makefile,v 1.5 2017/12/21 00:41:22 djm Exp $

PROG=test_utf8
SRCS=tests.c

# From usr.bin/ssh
SRCS+=utf8.c atomicio.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
