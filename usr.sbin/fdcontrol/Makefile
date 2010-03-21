# $FreeBSD: src/usr.sbin/fdcontrol/Makefile,v 1.12.24.1 2010/02/10 00:26:20 kensmith Exp $

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
