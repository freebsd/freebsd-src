# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.8.22.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	ncal

DPADD=	${LIBCALENDAR}
LDADD=	-lcalendar
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
