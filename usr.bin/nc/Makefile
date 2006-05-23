# $FreeBSD: src/usr.bin/nc/Makefile,v 1.3.2.1 2005/02/13 07:35:07 delphij Exp $

.PATH: ${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD=	-lipsec
DPADD=	${LIBIPSEC}

.include <bsd.prog.mk>
