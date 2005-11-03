# $FreeBSD: src/usr.sbin/fdformat/Makefile,v 1.13 2005/01/08 15:46:05 delphij Exp $

.PATH: ${.CURDIR}/../fdread

PROG=	fdformat
SRCS=	fdformat.c fdutil.c

WARNS?=	6
CFLAGS+= -I${.CURDIR}/../fdread

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
