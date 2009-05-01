# $FreeBSD: src/usr.sbin/gstat/Makefile,v 1.8.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	gstat
MAN=	gstat.8
WARNS?=	5
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} ${LIBEDIT} ${LIBCURSES}
LDADD=	-ldevstat -lkvm -lgeom -lbsdxml -lsbuf -ledit -lcurses

.include <bsd.prog.mk>
