# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.14.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
