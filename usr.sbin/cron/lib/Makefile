# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.8.8.1 2009/04/15 03:14:26 kensmith Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
