# $FreeBSD$

PACKAGE=lib${LIB}
LIB=	openbsd
SRCS=	getdtablecount.c \
	imsg-buffer.c \
	imsg.c \
	ohash.c
INTERNALLIB=

CFLAGS+= -I${.CURDIR}

WARNS=	3

.include <bsd.lib.mk>
