# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.6 2002/05/13 11:24:02 ru Exp $

LIB=	cron
INTERNALLIB= YES
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP

.include <bsd.lib.mk>
