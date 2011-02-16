# $FreeBSD: src/lib/libproc/Makefile,v 1.2.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $

LIB=	proc

SRCS=	proc_bkpt.c		\
	proc_create.c		\
	proc_regs.c		\
	proc_sym.c		\
	proc_rtld.c		\
	proc_util.c

INCS=	libproc.h

CFLAGS+=	-I. -I${.CURDIR}

SHLIB_MAJOR=	2

WARNS?=	6

WITHOUT_MAN=	yes

.include <bsd.lib.mk>
