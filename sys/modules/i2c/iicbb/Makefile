# $FreeBSD: src/sys/modules/i2c/iicbb/Makefile,v 1.1.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  iicbb_if.h iicbb_if.c iicbb.c

.include <bsd.kmod.mk>
