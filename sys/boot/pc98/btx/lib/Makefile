# $FreeBSD: src/sys/boot/pc98/btx/lib/Makefile,v 1.2.2.2 2002/12/01 08:31:57 nyan Exp $

OBJS=		btxcsu.o btxsys.o btxv86.o
CLEANFILES+=	crt0.o ${OBJS}

all: crt0.o

crt0.o: ${OBJS}
	${LD} ${LDFLAGS} -i -o ${.TARGET} ${OBJS}

.include <bsd.lib.mk>

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}
