# $FreeBSD: src/usr.sbin/ntp/libopts/Makefile,v 1.1.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/ntp/libopts 

LIB= opts
INTERNALLIB=

SRCS=	libopts.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
		 -I${.CURDIR}/../../../contrib/ntp/libopts

.include <bsd.lib.mk>
