# $FreeBSD: src/sys/modules/streams/Makefile,v 1.4.2.1 2001/07/24 09:49:43 dd Exp $ 

MAINTAINER  =  newton@atdot.dotat.org
.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif

.PATH:  ${.CURDIR}/../../dev/streams
KMOD=   streams
SRCS=   streams.c opt_streams.h

NOMAN=

CFLAGS+=	-O

.include <bsd.kmod.mk>
