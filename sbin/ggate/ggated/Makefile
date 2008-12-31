# $FreeBSD: src/sbin/ggate/ggated/Makefile,v 1.5.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

CFLAGS+= -I${.CURDIR}/../shared

.include <bsd.prog.mk>
