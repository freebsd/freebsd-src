# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.12 2002/04/28 12:54:24 markm Exp $

PROG=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif

.include <bsd.prog.mk>
