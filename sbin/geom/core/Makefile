# $FreeBSD$

.PATH: ${.CURDIR}/../misc

PROG=	geom
NOMAN=	notyet
SRCS=	geom.c subr.c

CFLAGS+= -DCLASSDIR=\"${CLASSDIR}\"
CFLAGS+= -I${.CURDIR}/../../../sys -I${.CURDIR} -I${.CURDIR}/..

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML}
LDADD=	-lgeom -lsbuf -lbsdxml

.include <bsd.prog.mk>
