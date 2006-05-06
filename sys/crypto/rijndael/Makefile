# $FreeBSD: src/sys/crypto/rijndael/Makefile,v 1.3 2004/12/21 08:47:18 ru Exp $

PROG=test00
NO_MAN=
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
