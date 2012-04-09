# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.5.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
