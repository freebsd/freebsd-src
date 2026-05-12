#	$OpenBSD: Makefile,v 1.6 2025/04/15 04:00:42 djm Exp $

PROG=test_match
SRCS=tests.c

# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=match.c misc.c log.c uidswap.c fatal.c ssherr.c addrmatch.c xmalloc.c
SRCS+=cleanup.c atomicio.c addr.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} ${UNITTEST_ARGS}

.include <bsd.regress.mk>
