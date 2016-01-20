#	$OpenBSD: Makefile,v 1.1 2014/06/24 01:14:18 djm Exp $

TEST_ENV=      "MALLOC_OPTIONS=AFGJPRX"

PROG=test_sshkey
SRCS=tests.c test_sshkey.c test_file.c test_fuzz.c common.c
REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} -d ${.CURDIR}/testdata

.include <bsd.regress.mk>

