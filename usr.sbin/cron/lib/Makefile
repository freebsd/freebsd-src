# $FreeBSD$

LIB=	cron
INTERNALLIB= YES
SRCS=	entry.c env.c misc.c
CFLAGS+=-I${.CURDIR}/../cron
CFLAGS+=-DLOGIN_CAP

.include <bsd.lib.mk>
