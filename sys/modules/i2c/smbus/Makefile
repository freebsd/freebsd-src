# $FreeBSD: src/sys/modules/i2c/smbus/Makefile,v 1.1.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/smbus
KMOD		= smbus
SRCS		= device_if.h bus_if.h smbus_if.h smbus_if.c \
		  smbconf.h smbconf.c smbus.h smbus.c

.include <bsd.kmod.mk>
