# $FreeBSD$ 
MAINTAINER=	newton@atdot.dotat.org

.PATH: ${.CURDIR}/../../dev/streams

KMOD=	streams
SRCS=	streams.c opt_streams.h
NOMAN=

.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif
CFLAGS+= -O

.include <bsd.kmod.mk>
