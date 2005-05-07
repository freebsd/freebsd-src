# $FreeBSD: src/usr.sbin/fdread/Makefile,v 1.4 2004/03/28 13:42:27 nyan Exp $

PROG=	fdread
SRCS=	fdread.c fdutil.c

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
