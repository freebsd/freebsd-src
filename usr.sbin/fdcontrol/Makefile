# $FreeBSD$

.PATH: ${.CURDIR:H}/fdread

PROG=	fdcontrol
SRCS=	fdcontrol.c fdutil.c
CFLAGS+= -I${.CURDIR:H}/fdread
MAN=	fdcontrol.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
