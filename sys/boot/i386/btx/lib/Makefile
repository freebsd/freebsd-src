# $FreeBSD: src/sys/boot/i386/btx/lib/Makefile,v 1.3 1999/08/28 00:40:07 peter Exp $

OBJS=		btxcsu.o btxsys.o btxv86.o
AFLAGS+=	-elf
LDFLAGS+=	-elf
CLEANFILES+=	crt0.o
INTERNALLIB=	true
NOMAN=		true
NOPIC=		true
NOPROFILE=	true

all: crt0.o

crt0.o: ${OBJS}
	${LD} ${LDFLAGS} -i -o ${.TARGET} ${OBJS}

.include <bsd.lib.mk>

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}
