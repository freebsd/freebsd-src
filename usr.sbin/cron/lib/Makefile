# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
