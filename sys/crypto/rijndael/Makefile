# $FreeBSD: src/sys/crypto/rijndael/Makefile,v 1.1 2003/10/19 22:12:23 phk Exp $

PROG=test00
NOMAN=1
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
