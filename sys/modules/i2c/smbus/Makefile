# $FreeBSD: src/sys/modules/i2c/smbus/Makefile,v 1.1.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/smbus
KMOD		= smbus
SRCS		= device_if.h bus_if.h smbus_if.h smbus_if.c \
		  smbconf.h smbconf.c smbus.h smbus.c

.include <bsd.kmod.mk>
