# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.11 1999/08/28 01:02:42 peter Exp $

PROG=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
CFLAGS+=	-Wall

.include <bsd.prog.mk>
