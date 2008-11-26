# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.13.8.1.2.1 2008/10/02 02:57:24 kensmith Exp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
