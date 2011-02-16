# $FreeBSD: src/usr.sbin/fdformat/Makefile,v 1.13.26.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../fdread

PROG=	fdformat
SRCS=	fdformat.c fdutil.c

WARNS?=	6
CFLAGS+= -I${.CURDIR}/../fdread

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
