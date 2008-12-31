# $FreeBSD: src/sys/modules/i2c/iicbb/Makefile,v 1.1.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  iicbb_if.h iicbb_if.c iicbb.c

.include <bsd.kmod.mk>
