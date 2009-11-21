# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.10.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	ncal

DPADD=	${LIBCALENDAR} ${LIBTERMCAP}
LDADD=	-lcalendar -ltermcap
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
