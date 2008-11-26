# $FreeBSD: src/sys/modules/i2c/iicbus/Makefile,v 1.1.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbus
SRCS		= device_if.h bus_if.h iicbus_if.h iicbus_if.c \
		  iiconf.h iiconf.c iicbus.h iicbus.c

.include <bsd.kmod.mk>
