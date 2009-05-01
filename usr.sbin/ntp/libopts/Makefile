# $FreeBSD: src/usr.sbin/ntp/libopts/Makefile,v 1.1.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/libopts 

LIB= opts
INTERNALLIB=

SRCS=	libopts.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
		 -I${.CURDIR}/../../../contrib/ntp/libopts

.include <bsd.lib.mk>
