# $FreeBSD: src/sys/boot/i386/btx/lib/Makefile,v 1.3.2.1 2002/07/19 18:46:28 ru Exp $

OBJS=		btxcsu.o btxsys.o btxv86.o
AFLAGS+=	-elf
LDFLAGS+=	-elf
CLEANFILES+=	crt0.o ${OBJS}

all: crt0.o

crt0.o: ${OBJS}
	${LD} ${LDFLAGS} -i -o ${.TARGET} ${OBJS}

.include <bsd.lib.mk>

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}
