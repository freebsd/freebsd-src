# $FreeBSD: src/usr.sbin/gstat/Makefile,v 1.8.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	gstat
MAN=	gstat.8
WARNS?=	5
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} ${LIBEDIT} ${LIBCURSES}
LDADD=	-ldevstat -lkvm -lgeom -lbsdxml -lsbuf -ledit -lcurses

.include <bsd.prog.mk>
