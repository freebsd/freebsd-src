# $FreeBSD: src/sys/boot/i386/btx/lib/Makefile,v 1.10 2002/09/17 01:48:55 peter Exp $

OBJS=		btxcsu.o btxsys.o btxv86.o
CLEANFILES+=	crt0.o ${OBJS}

all: crt0.o

crt0.o: ${OBJS}
	${LD} ${LDFLAGS} -i -o ${.TARGET} ${OBJS}

.include <bsd.lib.mk>

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}
