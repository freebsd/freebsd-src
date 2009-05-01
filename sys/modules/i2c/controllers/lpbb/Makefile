# $FreeBSD: src/sys/modules/i2c/controllers/lpbb/Makefile,v 1.2.30.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/ppbus
KMOD		= lpbb
SRCS		= device_if.h bus_if.h iicbb_if.h ppbus_if.h \
		  lpbb.c

.include <bsd.kmod.mk>
