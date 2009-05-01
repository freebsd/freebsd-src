# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.8.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	ncal

DPADD=	${LIBCALENDAR}
LDADD=	-lcalendar
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
