# $FreeBSD$

.PATH: ${.CURDIR}/../fdread

PROG=	fdformat
SRCS=	fdformat.c fdutil.c

WARNS?=	2
CFLAGS+= -I${.CURDIR}/../fdread

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
