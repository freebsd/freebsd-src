# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.14.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
