# $FreeBSD: src/sys/boot/pc98/btx/lib/Makefile,v 1.7 2002/09/19 13:43:02 nyan Exp $

OBJS=		btxcsu.o btxsys.o btxv86.o
CLEANFILES+=	crt0.o ${OBJS}

all: crt0.o

crt0.o: ${OBJS}
	${LD} ${LDFLAGS} -i -o ${.TARGET} ${OBJS}

.include <bsd.lib.mk>

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}
