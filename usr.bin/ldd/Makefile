#	$Id: Makefile,v 1.10 1998/09/16 09:31:48 dfr Exp $

PROG=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
CFLAGS+=	-Wall

.include <bsd.prog.mk>
