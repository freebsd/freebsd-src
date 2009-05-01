# $FreeBSD: src/sys/modules/i2c/iicbus/Makefile,v 1.1.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbus
SRCS		= device_if.h bus_if.h iicbus_if.h iicbus_if.c \
		  iiconf.h iiconf.c iicbus.h iicbus.c

.include <bsd.kmod.mk>
