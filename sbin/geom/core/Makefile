# $FreeBSD$

.PATH: ${.CURDIR}/../misc

PROG=	geom
NOMAN=	notyet
SRCS=	geom.c subr.c

CFLAGS+= -DCLASSDIR=\"${CLASSDIR}\"
CFLAGS+= -I${.CURDIR}/../../../sys -I${.CURDIR} -I${.CURDIR}/..

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil

.include <bsd.prog.mk>
