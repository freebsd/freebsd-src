# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.5.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
