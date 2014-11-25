# $FreeBSD$

LIB=		dpv
SHLIB_MAJOR=	1
INCS=		dpv.h
MAN=		dpv.3
MLINKS=		dpv.3 dpv_free.3

CFLAGS+=	-I${.CURDIR}
LDFLAGS+=	-ldialog -lfigpar -lncurses -lutil

SRCS=		dialog_util.c dialogrc.c dprompt.c dpv.c status.c util.c

WARNS?=		6

.include <bsd.lib.mk>
