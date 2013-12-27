# $FreeBSD$

PROG=	boot0cfg
MAN=	boot0cfg.8

DPADD=	${LIBGEOM} ${LIBBSDXML} ${LIBSBUF}
LDADD=	-lgeom -lbsdxml -lsbuf

NO_WCAST_ALIGN=

.include <bsd.prog.mk>
