#	$OpenBSD: Makefile,v 1.4 2017/12/21 03:01:49 djm Exp $

PROG=test_match
SRCS=tests.c

# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=match.c misc.c log.c uidswap.c fatal.c ssherr.c addrmatch.c xmalloc.c
SRCS+=cleanup.c atomicio.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
