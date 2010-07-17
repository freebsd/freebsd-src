# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.5.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
