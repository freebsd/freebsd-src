# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.8 2005/01/27 14:52:45 delphij Exp $

PROG=	ncal

DPADD=	${LIBCALENDAR}
LDADD=	-lcalendar
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
