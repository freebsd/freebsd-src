# $FreeBSD$

.PATH: ${.CURDIR}/../misc

PROG=	geom
NOMAN=	notyet
SRCS=	geom.c subr.c

CFLAGS+= -DCLASSDIR=\"${CLASSDIR}\"
CFLAGS+= -I${.CURDIR}/../../../sys -I${.CURDIR} -I${.CURDIR}/..

DPADD=	${LIBBSDXML} ${LIBGEOM} ${LIBSBUF} ${LIBUTIL}
LDADD=	-lbsdxml -lgeom -lsbuf -lutil

.include <bsd.prog.mk>
