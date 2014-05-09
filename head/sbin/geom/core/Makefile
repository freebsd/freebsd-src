# $FreeBSD$

.PATH: ${.CURDIR}/../misc

PROG=	geom
MAN=	geom.8
SRCS=	geom.c subr.c

NO_SHARED=NO

CFLAGS+= -DGEOM_CLASS_DIR=\"${GEOM_CLASS_DIR}\"
CFLAGS+= -I${.CURDIR}/../../../sys -I${.CURDIR} -I${.CURDIR}/..

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil

.include <bsd.prog.mk>
