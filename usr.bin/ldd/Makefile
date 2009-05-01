# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.13.18.1.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
