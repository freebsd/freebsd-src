# $FreeBSD$

PROG=	gconcat
MAN=	gconcat.8
DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML}
LDADD=	-lgeom -lsbuf -lbsdxml
WARNS?=	6
CFLAGS+=-I${.CURDIR}/../../sys

.include <bsd.prog.mk>
