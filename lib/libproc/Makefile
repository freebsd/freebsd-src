# $FreeBSD: src/lib/libproc/Makefile,v 1.1.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

LIB=	proc

SRCS=				\
	proc_create.c		\
	proc_sym.c		\
	proc_util.c

INCS=	libproc.h

CFLAGS+=	-I. -I${.CURDIR}

SHLIB_MAJOR=	1

WARNS?=	6

WITHOUT_MAN=	yes

.include <bsd.lib.mk>
