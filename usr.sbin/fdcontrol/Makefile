# $FreeBSD: src/usr.sbin/fdcontrol/Makefile,v 1.7 2003/04/04 17:49:14 obrien Exp $

.PATH: ${.CURDIR}/../fdread

PROG=	fdcontrol
SRCS=	fdcontrol.c fdutil.c
MAN=	fdcontrol.8

WARNS?=	2
CFLAGS+= -I${.CURDIR}/../fdread

.include <bsd.prog.mk>
