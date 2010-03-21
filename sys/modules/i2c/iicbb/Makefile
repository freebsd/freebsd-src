# $FreeBSD: src/sys/modules/i2c/iicbb/Makefile,v 1.1.38.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  iicbb_if.h iicbb_if.c iicbb.c

.include <bsd.kmod.mk>
