#	$OpenBSD: Makefile,v 1.6 2026/02/06 23:39:14 dtucker Exp $

PROG=test_conversion
SRCS=tests.c

# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=atomicio.c misc.c xmalloc.c log.c uidswap.c cleanup.c fatal.c ssherr.c
SRCS+=match.c addr.c addrmatch.c ssherr-libcrypto.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} ${UNITTEST_ARGS}

.include <bsd.regress.mk>
