# $FreeBSD$

.PATH: ${.CURDIR}/../fdread

PROG=	fdcontrol
SRCS=	fdcontrol.c fdutil.c
MAN=	fdcontrol.8

WARNS?=	2
CFLAGS+= -I${.CURDIR}/../fdread -static

.include <bsd.prog.mk>
