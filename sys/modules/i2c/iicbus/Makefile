# $FreeBSD: src/sys/modules/i2c/iicbus/Makefile,v 1.1.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbus
SRCS		= device_if.h bus_if.h iicbus_if.h iicbus_if.c \
		  iiconf.h iiconf.c iicbus.h iicbus.c

.include <bsd.kmod.mk>
