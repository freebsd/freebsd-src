# $FreeBSD$

PACKAGE=	dpv
LIB=		dpv
SHLIB_MAJOR=	3
INCS=		dpv.h
MAN=		dpv.3
MLINKS=		dpv.3 dpv_free.3

LIBADD=	dialog figpar util tinfow ncursesw

SRCS=		dialog_util.c dialogrc.c dprompt.c dpv.c status.c util.c

CFLAGS+=	-I${.CURDIR}

.include <bsd.lib.mk>
