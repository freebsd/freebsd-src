# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.5 2005/01/08 15:46:06 delphij Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
