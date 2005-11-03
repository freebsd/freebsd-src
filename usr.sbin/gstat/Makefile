# $FreeBSD: src/usr.sbin/gstat/Makefile,v 1.6 2005/01/27 14:52:46 delphij Exp $

PROG=	gstat
MAN=	gstat.8
WARNS?=	5
DPADD=	${LIBGEOM} ${LIBDEVSTAT} ${LIBKVM} ${LIBBSDXML} ${LIBCURSES} ${LIBSBUF}
LDADD=	-lgeom -ldevstat -lkvm -lbsdxml -lcurses -lsbuf

.include <bsd.prog.mk>
