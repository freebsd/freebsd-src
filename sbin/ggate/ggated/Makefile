# $FreeBSD: src/sbin/ggate/ggated/Makefile,v 1.5.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

CFLAGS+= -I${.CURDIR}/../shared

.include <bsd.prog.mk>
