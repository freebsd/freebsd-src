# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.13 2004/06/29 21:13:15 dwmalone Exp $

PROG=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
