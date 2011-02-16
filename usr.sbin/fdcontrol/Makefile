# $FreeBSD: src/usr.sbin/fdcontrol/Makefile,v 1.12.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../fdread

PROG=	fdcontrol
SRCS=	fdcontrol.c fdutil.c
WARNS?=	6
CFLAGS+= -I${.CURDIR}/../fdread
MAN=	fdcontrol.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
