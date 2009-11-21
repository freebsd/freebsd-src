# $FreeBSD: src/usr.sbin/gstat/Makefile,v 1.8.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	gstat
MAN=	gstat.8
WARNS?=	5
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} ${LIBEDIT} ${LIBCURSES}
LDADD=	-ldevstat -lkvm -lgeom -lbsdxml -lsbuf -ledit -lcurses

.include <bsd.prog.mk>
