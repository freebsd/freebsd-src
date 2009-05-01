# $FreeBSD: src/sys/crypto/rijndael/Makefile,v 1.3.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=test00
NO_MAN=
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
