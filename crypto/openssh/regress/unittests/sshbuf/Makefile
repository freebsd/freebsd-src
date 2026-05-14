#	$OpenBSD: Makefile,v 1.11 2026/02/06 23:39:14 dtucker Exp $

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
SRCS+=sshbuf-io.c atomicio.c misc.c xmalloc.c log.c fatal.c ssherr.c cleanup.c
SRCS+=match.c addr.c addrmatch.c ssherr-libcrypto.c

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} ${UNITTEST_ARGS}

