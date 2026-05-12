#	$OpenBSD: Makefile,v 1.6 2025/04/15 04:00:42 djm Exp $

PROG=test_utf8
SRCS=tests.c

# From usr.bin/ssh
SRCS+=utf8.c atomicio.c misc.c xmalloc.c match.c ssherr.c cleanup.c fatal.c
SRCS+=sshbuf.c sshbuf-getput-basic.c sshbuf-misc.c addr.c addrmatch.c log.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} ${UNITTEST_ARGS}

.include <bsd.regress.mk>
