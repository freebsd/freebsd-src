# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.4.6.1 2002/07/19 18:46:29 ru Exp $

LIB=	cron
INTERNALLIB= YES
SRCS=	entry.c env.c misc.c
CFLAGS+=-I${.CURDIR}/../cron
CFLAGS+=-DLOGIN_CAP

.include <bsd.lib.mk>
