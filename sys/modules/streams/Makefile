# $FreeBSD$ 

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
