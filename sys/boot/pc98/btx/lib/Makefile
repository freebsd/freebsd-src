# $FreeBSD: src/sys/boot/pc98/btx/lib/Makefile,v 1.8 2003/06/30 14:10:58 ru Exp $

OBJS=		btxcsu.o btxsys.o btxv86.o
CLEANFILES=	crt0.o

all: crt0.o

crt0.o: ${OBJS}
	${LD} ${LDFLAGS} -i -o ${.TARGET} ${OBJS}

.include <bsd.prog.mk>
