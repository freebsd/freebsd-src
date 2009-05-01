# $FreeBSD: src/usr.bin/nc/Makefile,v 1.3.22.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD=	-lipsec
DPADD=	${LIBIPSEC}

.include <bsd.prog.mk>
