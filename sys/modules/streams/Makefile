# $FreeBSD$ 

MAINTAINER  =  newton@atdot.dotat.org
.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif

.PATH:  ${.CURDIR}/../../dev/streams
KMOD=   streams
SRCS=   streams.c opt_streams.h opt_devfs.h

NOMAN=

CFLAGS+=	-O

.include "${.CURDIR}/../../conf/kmod.mk"
