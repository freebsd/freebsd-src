# $FreeBSD: src/sbin/ggate/ggated/Makefile,v 1.5.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

CFLAGS+= -I${.CURDIR}/../shared

.include <bsd.prog.mk>
