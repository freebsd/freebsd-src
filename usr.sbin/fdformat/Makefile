# $FreeBSD$

.PATH: ${.CURDIR:H}/fdread

PROG=	fdformat
SRCS=	fdformat.c fdutil.c

CFLAGS+= -I${.CURDIR:H}/fdread

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
