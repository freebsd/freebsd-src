# $FreeBSD: src/usr.sbin/fdformat/Makefile,v 1.12 2004/03/28 13:42:27 nyan Exp $

.PATH: ${.CURDIR}/../fdread

PROG=	fdformat
SRCS=	fdformat.c fdutil.c

WARNS?=	2
CFLAGS+= -I${.CURDIR}/../fdread

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
