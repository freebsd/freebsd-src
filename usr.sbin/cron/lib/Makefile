# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.10.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

WARNS?=	3

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
