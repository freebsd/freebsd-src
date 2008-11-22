# $FreeBSD$

PROG=	ncal

DPADD=	${LIBCALENDAR}
LDADD=	-lcalendar
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
