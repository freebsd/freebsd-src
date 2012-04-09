# $FreeBSD: src/sys/modules/i2c/iicbus/Makefile,v 1.1.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbus
SRCS		= device_if.h bus_if.h iicbus_if.h iicbus_if.c \
		  iiconf.h iiconf.c iicbus.h iicbus.c

.include <bsd.kmod.mk>
