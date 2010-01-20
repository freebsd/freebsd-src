# $FreeBSD$

PROG=		mdconfig
MAN=		mdconfig.8
MLINKS=		mdconfig.8 vnconfig.8

WARNS?=	2
DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} 
LDADD=	-lutil -lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>
