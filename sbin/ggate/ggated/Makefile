# $FreeBSD: src/sbin/ggate/ggated/Makefile,v 1.5.26.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

CFLAGS+= -I${.CURDIR}/../shared

.include <bsd.prog.mk>
