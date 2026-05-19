#	$OpenBSD: Makefile,v 1.6 2026/02/06 23:39:14 dtucker Exp $

PROG=test_bitmap
SRCS=tests.c

# From usr.sbin/ssh
SRCS+=bitmap.c atomicio.c misc.c xmalloc.c fatal.c log.c cleanup.c match.c
SRCS+=sshbuf.c sshbuf-getput-basic.c sshbuf-misc.c ssherr.c addr.c addrmatch.c
SRCS+=ssherr-libcrypto.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} ${UNITTEST_ARGS}

.include <bsd.regress.mk>
