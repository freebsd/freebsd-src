# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.13.18.1.8.1 2010/12/21 17:10:29 kensmith Exp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
