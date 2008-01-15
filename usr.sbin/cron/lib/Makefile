# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.7 2004/10/24 15:33:02 ru Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP

.include <bsd.lib.mk>
