# $FreeBSD: src/sys/crypto/rijndael/Makefile,v 1.3.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=test00
NO_MAN=
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
