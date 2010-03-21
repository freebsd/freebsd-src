# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.13.18.1.6.1 2010/02/10 00:26:20 kensmith Exp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
