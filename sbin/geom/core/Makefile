# $FreeBSD$

.PATH:	${.CURDIR}/../misc

PROG=	geom
SRCS=	geom.c subr.c
#MAN=	geom.8
NOMAN=	notyet
DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML}
LDADD=	-lgeom -lsbuf -lbsdxml
WARNS?=	6
CFLAGS+=-I${.CURDIR}/../../../sys -I${.CURDIR} -I${.CURDIR}/..
CFLAGS+=-DCLASSDIR=\"${LIBDIR}\"

.include <bsd.prog.mk>
