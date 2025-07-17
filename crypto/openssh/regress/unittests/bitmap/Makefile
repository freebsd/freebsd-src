#	$OpenBSD: Makefile,v 1.4 2017/12/21 00:41:22 djm Exp $

PROG=test_bitmap
SRCS=tests.c

# From usr.sbin/ssh
SRCS+=bitmap.c atomicio.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
