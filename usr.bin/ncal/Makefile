# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.5 2004/02/05 22:44:25 ru Exp $

PROG=	ncal
SRCS=	ncal.c

DPADD=	${LIBCALENDAR}
LDADD=	-lcalendar

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
