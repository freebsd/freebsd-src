# $FreeBSD: src/usr.bin/nc/Makefile,v 1.3.24.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD=	-lipsec
DPADD=	${LIBIPSEC}

.include <bsd.prog.mk>
