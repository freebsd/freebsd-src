# $FreeBSD: src/sys/boot/i386/btx/lib/Makefile,v 1.11 2003/06/30 14:10:57 ru Exp $

OBJS=		btxcsu.o btxsys.o btxv86.o
CLEANFILES=	crt0.o

all: crt0.o

crt0.o: ${OBJS}
	${LD} ${LDFLAGS} -i -o ${.TARGET} ${OBJS}

.include <bsd.prog.mk>
