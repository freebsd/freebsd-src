# $FreeBSD: src/sys/crypto/rijndael/Makefile,v 1.3.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=test00
NO_MAN=
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
