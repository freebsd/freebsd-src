#	$Id: Makefile,v 1.9 1998/06/12 14:11:15 peter Exp $

PROG=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
CFLAGS+=	-Wall

.include <bsd.prog.mk>
