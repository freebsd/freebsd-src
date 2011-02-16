# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.14.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
