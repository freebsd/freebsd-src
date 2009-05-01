# $FreeBSD: src/sbin/ggate/ggated/Makefile,v 1.5.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

CFLAGS+= -I${.CURDIR}/../shared

.include <bsd.prog.mk>
