# $FreeBSD$

.PATH: ${.CURDIR}/../fdread

PROG=	fdcontrol
SRCS=	fdcontrol.c fdutil.c
CFLAGS+= -I${.CURDIR}/../fdread
WARNS?=	2
MAN=	fdcontrol.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
