# $FreeBSD: src/sys/modules/i2c/controllers/lpbb/Makefile,v 1.2.32.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/ppbus
KMOD		= lpbb
SRCS		= device_if.h bus_if.h iicbb_if.h ppbus_if.h \
		  lpbb.c

.include <bsd.kmod.mk>
