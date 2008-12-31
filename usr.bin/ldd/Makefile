# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.13.18.1.2.1 2008/11/25 02:59:29 kensmith Exp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
