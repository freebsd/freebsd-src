# $FreeBSD: src/sys/modules/i2c/iicbb/Makefile,v 1.1.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  iicbb_if.h iicbb_if.c iicbb.c

.include <bsd.kmod.mk>
