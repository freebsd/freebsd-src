# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.8.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	ncal

DPADD=	${LIBCALENDAR}
LDADD=	-lcalendar
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
