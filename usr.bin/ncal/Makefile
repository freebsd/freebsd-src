# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.4 2002/04/28 12:24:20 markm Exp $

PROG=	ncal
SRCS=	ncal.c

DPADD+=	${LIBCALENDAR}
LDADD+=	-lcalendar

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
