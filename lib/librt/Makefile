# $FreeBSD$

LIB=rt
SHLIB_MAJOR= 1
CFLAGS+=-I${.CURDIR}/../libc/include -I${.CURDIR}
CFLAGS+=-Winline -Wall -g

#MAN=	libthr.3

SRCS+= aio.c mq.c sigev_thread.c timer.c

PRECIOUSLIB=

.include <bsd.lib.mk>
