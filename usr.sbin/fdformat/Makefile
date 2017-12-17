# $FreeBSD$

.PATH: ${.CURDIR}/../fdread

PROG=	fdformat
MAN=	fdformat.8
SRCS=	fdformat.c fdutil.c

CFLAGS+= -I${.CURDIR}/../fdread

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
