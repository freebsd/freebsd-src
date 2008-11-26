# $FreeBSD: src/sys/modules/i2c/controllers/pcf/Makefile,v 1.2.20.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:		${.CURDIR}/../../../../i386/isa
KMOD		= pcf
SRCS		= device_if.h bus_if.h iicbus_if.h isa_if.h \
		  pcf.c

.include <bsd.kmod.mk>
