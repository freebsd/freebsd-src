# $FreeBSD: src/sys/modules/i2c/controllers/pcf/Makefile,v 1.1 2002/03/23 15:48:45 nsouch Exp $

.PATH:		${.CURDIR}/../../../../i386/isa
KMOD		= pcf
SRCS		= device_if.h bus_if.h iicbus_if.h isa_if.h \
		  opt_compat_oldisa.h pcf.c

.include <bsd.kmod.mk>
