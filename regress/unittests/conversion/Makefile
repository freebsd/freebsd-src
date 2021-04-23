#	$OpenBSD: Makefile,v 1.4 2021/01/09 12:24:30 dtucker Exp $

PROG=test_conversion
SRCS=tests.c

# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=atomicio.c misc.c xmalloc.c log.c uidswap.c cleanup.c fatal.c ssherr.c
SRCS+=match.c addr.c addrmatch.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
