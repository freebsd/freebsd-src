#	$OpenBSD: Makefile,v 1.2 2016/05/30 12:14:08 schwarze Exp $

TEST_ENV=	"MALLOC_OPTIONS=CFGJPRSUX"

PROG=test_utf8
SRCS=tests.c
REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
