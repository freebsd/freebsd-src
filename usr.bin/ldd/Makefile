# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.14.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
