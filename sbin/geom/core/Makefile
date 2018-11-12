# $FreeBSD$

.PATH: ${.CURDIR}/../misc

PACKAGE=runtime
PROG=	geom
MAN=	geom.8
SRCS=	geom.c subr.c

NO_SHARED=NO

CFLAGS+= -DGEOM_CLASS_DIR=\"${GEOM_CLASS_DIR}\"
CFLAGS+= -I${.CURDIR}/../../../sys -I${.CURDIR} -I${.CURDIR}/..

LIBADD=	geom util

.include <bsd.prog.mk>
