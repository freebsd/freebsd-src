# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.10.2.2.4.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	ncal

DPADD=	${LIBCALENDAR} ${LIBTERMCAP}
LDADD=	-lcalendar -ltermcap

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
