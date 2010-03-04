# $FreeBSD$

PROG=	ncal

DPADD=	${LIBCALENDAR} ${LIBTERMCAP}
LDADD=	-lcalendar -ltermcap
WARNS?=	6

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
