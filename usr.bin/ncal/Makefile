# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.10.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	ncal

DPADD=	${LIBCALENDAR} ${LIBTERMCAP}
LDADD=	-lcalendar -ltermcap
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
