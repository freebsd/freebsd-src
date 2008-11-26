# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.8.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	ncal

DPADD=	${LIBCALENDAR}
LDADD=	-lcalendar
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
