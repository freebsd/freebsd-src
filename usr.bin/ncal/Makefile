#	$Id: Makefile,v 1.1.1.1.2.1 1998/01/12 05:10:59 obrien Exp $
PROG= ncal
SRCS= ncal.c

CFLAGS+=	-Wall -Wmissing-prototypes -fstrict-prototypes -ansi -pedantic

DPADD+=		${LIBCALENDAR}
LDADD+=		-lcalendar

LINKS=		${BINDIR}/ncal ${BINDIR}/cal
MLINKS=		ncal.1 cal.1

.include <bsd.prog.mk>
