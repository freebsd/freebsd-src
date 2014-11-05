# $FreeBSD$

LIB=		dpv
SHLIB_MAJOR=	1
INCS=		dpv.h
MAN=		dpv.3
MLINKS=		dpv.3 dpv_free.3

DPADD=		${LIBDIALOG} ${LIBFIGPAR} ${LIBNCURSESW} ${LIBUTIL}
LDADD=		-ldialog -lfigpar -lncursesw -lutil

SRCS=		dialog_util.c dialogrc.c dprompt.c dpv.c status.c util.c

CFLAGS+=	-I${.CURDIR}

WARNS?=		6

.include <bsd.lib.mk>
