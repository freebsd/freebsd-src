# $FreeBSD: src/sys/modules/i2c/iicbus/Makefile,v 1.1.40.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbus
SRCS		= device_if.h bus_if.h iicbus_if.h iicbus_if.c \
		  iiconf.h iiconf.c iicbus.h iicbus.c

.include <bsd.kmod.mk>
