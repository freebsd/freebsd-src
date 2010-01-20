# $FreeBSD$

PROG=	gstat
MAN=	gstat.8
WARNS?=	5
DPADD=	${LIBGEOM} ${LIBDEVSTAT} ${LIBBSDXML} ${LIBCURSES} ${LIBEDIT}
LDADD=	-lgeom -ldevstat -lbsdxml -lcurses -ledit

.include <bsd.prog.mk>
