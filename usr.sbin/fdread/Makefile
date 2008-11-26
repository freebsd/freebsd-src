# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.5.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
