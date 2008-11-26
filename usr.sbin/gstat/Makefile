# $FreeBSD: src/usr.sbin/gstat/Makefile,v 1.6.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	gstat
MAN=	gstat.8
WARNS?=	5
DPADD=	${LIBGEOM} ${LIBDEVSTAT} ${LIBBSDXML} ${LIBCURSES} ${LIBEDIT}
LDADD=	-lgeom -ldevstat -lbsdxml -lcurses -ledit

.include <bsd.prog.mk>
