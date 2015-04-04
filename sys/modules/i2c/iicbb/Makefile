# $FreeBSD$

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  iicbb_if.h iicbb_if.c iicbb.c opt_platform.h \
		  ofw_bus_if.h

.include <bsd.kmod.mk>
