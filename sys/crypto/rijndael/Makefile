# $FreeBSD: src/sys/crypto/rijndael/Makefile,v 1.3.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=test00
NO_MAN=
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
