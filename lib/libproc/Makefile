# $FreeBSD$

LIB=	proc

SRCS=				\
	proc_create.c		\
	proc_sym.c		\
	proc_util.c

INCS=	libproc.h

CFLAGS+=	-I. -I${.CURDIR}

SHLIB_MAJOR=	2

WITHOUT_MAN=	yes

.include <bsd.lib.mk>
