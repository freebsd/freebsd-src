# $FreeBSD: src/usr.sbin/fdcontrol/Makefile,v 1.10 2004/03/28 13:42:27 nyan Exp $

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
