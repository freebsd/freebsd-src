# $FreeBSD$

.PATH: ${.CURDIR}/../shared

PROG=	ggatec
MAN=	ggatec.8
SRCS=	ggatec.c ggate.c

CFLAGS+= -DLIBGEOM
CFLAGS+= -I${.CURDIR}/../shared

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML}
LDADD=	-lgeom -lsbuf -lbsdxml

.include <bsd.prog.mk>
