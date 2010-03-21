# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.5.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
