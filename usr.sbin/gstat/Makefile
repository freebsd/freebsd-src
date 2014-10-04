# $FreeBSD$

PROG=	gstat
MAN=	gstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} ${LIBEDIT} ${LIBNCURSESW}
LDADD=	-ldevstat -lkvm -lgeom -lbsdxml -lsbuf -ledit -lncursesw

.include <bsd.prog.mk>
