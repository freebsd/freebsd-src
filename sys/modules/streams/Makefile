# $FreeBSD: src/sys/modules/streams/Makefile,v 1.4 1999/11/28 18:53:23 bde Exp $ 

MAINTAINER  =  newton@atdot.dotat.org
.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif

.PATH:  ${.CURDIR}/../../dev/streams
KMOD=   streams
SRCS=   streams.c opt_streams.h opt_devfs.h

NOMAN=

CFLAGS+=	-O

.include <bsd.kmod.mk>
