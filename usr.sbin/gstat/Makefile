# $FreeBSD$

PROG=	gstat
NOMAN=
SRCS=	gstat.c
WARNS?=	5
DPADD=	${LIBGEOM} ${LIBDEVSTAT} ${LIBKVM} ${LIBBSDXML} ${LIBCURSES} ${LIBSBUF}
LDADD=	-lgeom -ldevstat -lkvm -lbsdxml -lcurses -lsbuf

.include <bsd.prog.mk>
