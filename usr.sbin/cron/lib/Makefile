# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.7.12.1 2008/10/02 02:57:24 kensmith Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP

.include <bsd.lib.mk>
