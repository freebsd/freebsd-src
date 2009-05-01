# $FreeBSD: src/usr.sbin/fdformat/Makefile,v 1.13.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../fdread

PROG=	fdformat
SRCS=	fdformat.c fdutil.c

WARNS?=	6
CFLAGS+= -I${.CURDIR}/../fdread

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
