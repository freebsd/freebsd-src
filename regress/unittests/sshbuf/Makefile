#	$OpenBSD: Makefile,v 1.7 2018/10/17 23:28:05 djm Exp $

.include <bsd.regress.mk>

PROG=test_sshbuf
SRCS=tests.c
SRCS+=test_sshbuf.c
SRCS+=test_sshbuf_getput_basic.c
SRCS+=test_sshbuf_getput_crypto.c
SRCS+=test_sshbuf_misc.c
SRCS+=test_sshbuf_fuzz.c
SRCS+=test_sshbuf_getput_fuzz.c
SRCS+=test_sshbuf_fixed.c

# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=atomicio.c

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} ${UNITTEST_ARGS}

