# $FreeBSD$

.if defined(RESCUE)

.PATH:	${.CURDIR}/class/part ${.CURDIR}/core ${.CURDIR}/misc

PROG=	geom
SRCS=	geom.c geom_part.c subr.c

CFLAGS+=-I${.CURDIR} -I${.CURDIR}/core

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil

.include <bsd.prog.mk>

.else

SUBDIR=	core class

.include <bsd.subdir.mk>

.endif
