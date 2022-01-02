# $FreeBSD$

.PATH:		${SRCTOP}/sys/dev/iicbus
KMOD		= iicbb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  iicbb_if.h iicbb_if.c iicbb.c ofw_bus_if.h opt_platform.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
