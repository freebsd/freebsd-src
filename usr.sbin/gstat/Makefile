# $FreeBSD$

PROG=	gstat
MAN=	gstat.8
SRCS=	gstat.c
WARNS?=	5
DPADD=	${LIBGEOM} ${LIBDEVSTAT} ${LIBKVM} ${LIBBSDXML} ${LIBCURSES} ${LIBSBUF}
LDADD=	-lgeom -ldevstat -lkvm -lbsdxml -lcurses -lsbuf

.include <bsd.prog.mk>
