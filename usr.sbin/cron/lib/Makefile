# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.8.10.1 2010/02/10 00:26:20 kensmith Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
