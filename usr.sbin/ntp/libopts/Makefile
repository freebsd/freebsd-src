# $FreeBSD: src/usr.sbin/ntp/libopts/Makefile,v 1.1.6.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/libopts 

LIB= opts
INTERNALLIB=

SRCS=	libopts.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
		 -I${.CURDIR}/../../../contrib/ntp/libopts

.include <bsd.lib.mk>
