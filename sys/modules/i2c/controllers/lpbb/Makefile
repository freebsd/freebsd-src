# $FreeBSD: src/sys/modules/i2c/controllers/lpbb/Makefile,v 1.2.28.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/ppbus
KMOD		= lpbb
SRCS		= device_if.h bus_if.h iicbb_if.h ppbus_if.h \
		  lpbb.c

.include <bsd.kmod.mk>
