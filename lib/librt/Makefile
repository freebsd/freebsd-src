# $FreeBSD: src/lib/librt/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $

LIB=rt
SHLIB_MAJOR= 1
CFLAGS+=-I${.CURDIR}/../libc/include -I${.CURDIR}
CFLAGS+=-Winline -Wall -g

#MAN=	libthr.3

SRCS+= aio.c mq.c sigev_thread.c timer.c

PRECIOUSLIB=

VERSION_MAP=	${.CURDIR}/Version.map

.include <bsd.lib.mk>
