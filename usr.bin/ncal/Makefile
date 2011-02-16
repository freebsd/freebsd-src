# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.8.10.1.2.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	ncal

DPADD=	${LIBCALENDAR} ${LIBTERMCAP}
LDADD=	-lcalendar -ltermcap

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
