# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.5.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
