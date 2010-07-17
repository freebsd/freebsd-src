# $FreeBSD: src/sbin/geom/core/Makefile,v 1.10.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../misc

PROG=	geom
MAN=	geom.8
SRCS=	geom.c subr.c

NO_SHARED=NO

CFLAGS+= -DCLASS_DIR=\"${CLASS_DIR}\"
CFLAGS+= -I${.CURDIR}/../../../sys -I${.CURDIR} -I${.CURDIR}/..

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil

.include <bsd.prog.mk>
