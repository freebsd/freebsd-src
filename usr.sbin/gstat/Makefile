# $FreeBSD$

.PATH: ${.CURDIR}/../../sys/kern

PROG=	gstat
NOMAN=
SRCS=	gstat.c subr_sbuf.c
WARNS?=	5
DPADD=	${LIBGEOM} ${LIBDEVSTAT} ${LIBKVM} ${LIBBSDXML} ${LIBCURSES}
LDADD=	-lgeom -ldevstat -lkvm -lbsdxml -lcurses

.include <bsd.prog.mk>
