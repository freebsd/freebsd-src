#	$OpenBSD: Makefile,v 1.1 2014/04/30 05:32:00 djm Exp $

PROG=test_sshbuf
SRCS=tests.c
SRCS+=test_sshbuf.c
SRCS+=test_sshbuf_getput_basic.c
SRCS+=test_sshbuf_getput_crypto.c
SRCS+=test_sshbuf_misc.c
SRCS+=test_sshbuf_fuzz.c
SRCS+=test_sshbuf_getput_fuzz.c
SRCS+=test_sshbuf_fixed.c

.include <bsd.regress.mk>

