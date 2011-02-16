# $FreeBSD: src/lib/librt/Makefile,v 1.5.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

LIB=rt
SHLIB_MAJOR= 1
CFLAGS+=-I${.CURDIR}/../libc/include -I${.CURDIR}
CFLAGS+=-Winline -Wall -g

#MAN=	libthr.3

SRCS+= aio.c mq.c sigev_thread.c timer.c

PRECIOUSLIB=

VERSION_MAP=	${.CURDIR}/Version.map

.include <bsd.lib.mk>
