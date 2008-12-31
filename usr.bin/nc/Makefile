# $FreeBSD: src/usr.bin/nc/Makefile,v 1.3.20.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD=	-lipsec
DPADD=	${LIBIPSEC}

.include <bsd.prog.mk>
