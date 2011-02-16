# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.5.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
