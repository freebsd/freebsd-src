# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.5.2.1 2004/11/30 16:47:27 wollman Exp $

PROG=	ncal
SRCS=	ncal.c

DPADD=	${LIBCALENDAR}
LDADD=	-lcalendar
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
