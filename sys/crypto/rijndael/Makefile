# $FreeBSD: src/sys/crypto/rijndael/Makefile,v 1.1.4.1 2005/02/13 07:23:32 obrien Exp $

PROG=test00
NOMAN=
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
