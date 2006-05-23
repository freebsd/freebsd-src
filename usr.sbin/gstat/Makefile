# $FreeBSD: src/usr.sbin/gstat/Makefile,v 1.5 2003/04/28 11:54:01 tjr Exp $

PROG=	gstat
MAN=	gstat.8
SRCS=	gstat.c
WARNS?=	5
DPADD=	${LIBGEOM} ${LIBDEVSTAT} ${LIBKVM} ${LIBBSDXML} ${LIBCURSES} ${LIBSBUF}
LDADD=	-lgeom -ldevstat -lkvm -lbsdxml -lcurses -lsbuf

.include <bsd.prog.mk>
