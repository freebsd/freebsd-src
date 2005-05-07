# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.6.8.1 2005/02/13 07:23:40 obrien Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP

.include <bsd.lib.mk>
