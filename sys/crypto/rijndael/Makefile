# $FreeBSD: src/sys/crypto/rijndael/Makefile,v 1.3.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=test00
NO_MAN=
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
