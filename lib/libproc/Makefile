# $FreeBSD: src/lib/libproc/Makefile,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

LIB=	proc

SRCS=				\
	proc_create.c		\
	proc_sym.c		\
	proc_util.c

INCS=	libproc.h

CFLAGS+=	-I. -I${.CURDIR}

SHLIB_MAJOR=	2

WARNS?=	6

WITHOUT_MAN=	yes

.include <bsd.lib.mk>
