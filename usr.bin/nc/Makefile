# $FreeBSD: src/usr.bin/nc/Makefile,v 1.3.28.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD=	-lipsec
DPADD=	${LIBIPSEC}

.include <bsd.prog.mk>
