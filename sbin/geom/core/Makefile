# $FreeBSD$

.PATH: ${.CURDIR}/../misc

PROG=	geom
MAN=	geom.8
SRCS=	geom.c subr.c

NOSHARED=NO

CFLAGS+= -DCLASS_DIR=\"${CLASS_DIR}\"
CFLAGS+= -I${.CURDIR}/../../../sys -I${.CURDIR} -I${.CURDIR}/..

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil

.include <bsd.prog.mk>
