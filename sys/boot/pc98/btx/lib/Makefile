#	$Id: Makefile,v 1.2 1998/10/11 11:27:48 rnordier Exp $

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
