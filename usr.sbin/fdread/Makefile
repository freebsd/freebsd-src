# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.5.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
