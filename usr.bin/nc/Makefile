# $FreeBSD: src/usr.bin/nc/Makefile,v 1.3.18.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD=	-lipsec
DPADD=	${LIBIPSEC}

.include <bsd.prog.mk>
