# $FreeBSD: src/usr.bin/ldd/Makefile,v 1.10.2.1 1999/08/29 15:29:19 peter Exp $

PROG=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
CFLAGS+=	-Wall

.include <bsd.prog.mk>
