# $FreeBSD: src/usr.sbin/fdcontrol/Makefile,v 1.6 2001/12/15 19:08:58 joerg Exp $

.PATH:	${.CURDIR}/../fdread

PROG=	fdcontrol
SRCS=	fdcontrol.c fdutil.c
MAN=	fdcontrol.8

WARNS?=	2
CFLAGS+=	-I${.CURDIR}/../fdread

.include <bsd.prog.mk>
