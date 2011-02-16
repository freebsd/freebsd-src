# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.8.12.1 2010/12/21 17:10:29 kensmith Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
