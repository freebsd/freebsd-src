# $FreeBSD$

.if defined(RESCUE) || defined(RELEASE_CRUNCH)

.PATH:	${.CURDIR}/class/part \
	${.CURDIR}/class/label \
	${.CURDIR}/core \
	${.CURDIR}/misc

PROG=	geom
SRCS=	geom.c geom_label.c geom_part.c subr.c
NO_MAN=

WARNS?=	2
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/core -DSTATIC_GEOM_CLASSES

DPADD=	${LIBGEOM} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL}
LDADD=	-lgeom -lsbuf -lbsdxml -lutil

.include <bsd.prog.mk>

.else

SUBDIR=	core class

.include <bsd.subdir.mk>

.endif
