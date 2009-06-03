# $FreeBSD$

.include <bsd.own.mk>

# XXX: LLVM PR3678
.if ${MK_CLANG_IS_CC} != "no" && ${CC} == "cc"
CC=	gcc
.endif

LIB=rt
SHLIB_MAJOR= 1
CFLAGS+=-I${.CURDIR}/../libc/include -I${.CURDIR}
CFLAGS+=-Winline -Wall -g

#MAN=	libthr.3

SRCS+= aio.c mq.c sigev_thread.c timer.c

PRECIOUSLIB=

VERSION_MAP=	${.CURDIR}/Version.map

.include <bsd.lib.mk>
