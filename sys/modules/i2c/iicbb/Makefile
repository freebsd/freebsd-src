# $FreeBSD: src/sys/modules/i2c/iicbb/Makefile,v 1.1.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  iicbb_if.h iicbb_if.c iicbb.c

.include <bsd.kmod.mk>
