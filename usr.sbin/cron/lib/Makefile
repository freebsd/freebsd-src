# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.8 2007/06/17 17:25:53 yar Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
