# $FreeBSD$

OBJS=		btxcsu.o btxsys.o btxv86.o
CLEANFILES=	crt0.o

all: crt0.o

crt0.o: ${OBJS}
	${LD} ${LDFLAGS} -i -o ${.TARGET} ${OBJS}

.include <bsd.prog.mk>
